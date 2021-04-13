#pragma once 

#include "../common.h"
#include "../light_hal.h"
#include "color_instant_handler.h"
#include "color_transition_handler.h"
#include "esphome/components/ledc/ledc_output.h"

namespace esphome {
namespace yeelight {
namespace bs2 {

/**
 * A LightOutput class for the Yeelight Bedside Lamp 2.
 *
 * The function of this class is to translate a required light state
 * into actual physicial GPIO output signals to drive the device's LED
 * circuitry. It forms the glue between the physical device and the
 * logical light color input.
 */
class YeelightBS2LightOutput : public Component, public light::LightOutput {
public:
    void set_parent(LightHAL *light) { light_ = light; }

    /**
     * Returns a LightTraits object, which is used to explain to the outside
     * world (e.g. Home Assistant) what features are supported by this device.
     */
    light::LightTraits get_traits() override
    {
        auto traits = light::LightTraits();
        traits.set_supports_rgb(true);
        traits.set_supports_color_temperature(true);
        traits.set_supports_brightness(true);
        traits.set_supports_rgb_white_value(false);
        traits.set_supports_color_interlock(true);
        traits.set_min_mireds(MIRED_MIN);
        traits.set_max_mireds(MIRED_MAX);
        return traits;
    }

    void add_on_state_callback(std::function<void(light::LightColorValues)> &&callback) {
          state_callback_.add(std::move(callback));
    }

    /**
     * Applies a requested light state to the physicial GPIO outputs.
     */
    void write_state(light::LightState *state)
    {
        auto values = state->current_values;

        // The color must either be set instantly, or the color is
        // transitioning to an end color. The transition handler will do its
        // own inspection to see if a transition is currently active or not.
        // Based on the outcome, use either the instant or transition handler.
        GPIOOutputs *delegate;
        if (transition_handler_->set_light_color_values(values)) {
            delegate = transition_handler_;
            state_callback_.call(transition_handler_->get_end_values());
        } else {
            instant_handler_->set_light_color_values(values);
            delegate = instant_handler_;
            state_callback_.call(values);
        }

        // Note: one might think that it is more logical to turn on the LED
        // circuitry master switch after setting the individual channels,
        // but this is the order that was used by the original firmware. I
        // tried to stay as close as possible to the original behavior, so
        // that's why these GPIOs are turned on at this point.
        if (values.get_state() != 0)
            light_->turn_on();

        // Apply the current GPIO output levels from the selected handler.
        light_->set_rgbw(
            delegate->red,
            delegate->green,
            delegate->blue,
            delegate->white
        );

        if (values.get_state() == 0)
            light_->turn_off();
    }

protected:
    LightHAL *light_;
    ColorTransitionHandler *transition_handler_;
    ColorInstantHandler *instant_handler_ = new ColorInstantHandler();
    CallbackManager<void(light::LightColorValues)> state_callback_{};

    friend class YeelightBS2LightState;

    /**
     * Called by the YeelightBS2LightState class, to set the object that can be
     * used to access the protected LightTransformer data from the LightState
     * object.
     */
    void set_transformer_inspector(LightStateTransformerInspector *exposer) {
        transition_handler_ = new ColorTransitionHandler(exposer);
    }
};

/**
 * This custom LightState class is used to provide access to the protected
 * LightTranformer information in the LightState class.
 *
 * This class is used by the ColorTransitionHandler class to inspect if
 * an ongoing light color transition is active in a LightState object.
 */
class YeelightBS2LightState : public light::LightState, public LightStateTransformerInspector
{
public:
    YeelightBS2LightState(const std::string &name, YeelightBS2LightOutput *output) : light::LightState(name, output) {
        output->set_transformer_inspector(this);
    }

    bool is_active() { return transformer_ != nullptr; }
    bool is_transition() { return transformer_->is_transition(); }
    light::LightColorValues get_end_values() { return transformer_->get_end_values(); }
    float get_progress() { return transformer_->get_progress(); }
};
    
} // namespace bs2
} // namespace yeelight
} // namespace esphome
