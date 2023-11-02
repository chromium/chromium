// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_COLOR_TEMPERATURE_ANIMATION_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_COLOR_TEMPERATURE_ANIMATION_H_

#include "chromecast/ui/display_settings_manager.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"

namespace chromecast {

namespace shell {
class CastDisplayConfigurator;
}

// Defines a linear animation type to animate the color temperature between two
// values in a given time duration.
class ColorTemperatureAnimation : public gfx::LinearAnimation {
 public:
  ColorTemperatureAnimation(
      shell::CastDisplayConfigurator* display_configurator,
      const DisplaySettingsManager::ColorTemperatureConfig& config);
  ColorTemperatureAnimation(const ColorTemperatureAnimation&) = delete;
  ColorTemperatureAnimation& operator=(const ColorTemperatureAnimation&) =
      delete;
  ~ColorTemperatureAnimation() override;

  // Starts a new temperature animation from the |current_temperature_| to the
  // given |new_target_temperature| in the given |duration|.
  void AnimateToNewValue(float new_target_temperature,
                         base::TimeDelta duration);

  // Animate to a neutral temperature value, with no color transformations
  // applied.
  void AnimateToNeutral(base::TimeDelta duration);

 private:
  // gfx::LinearAnimation implementation:
  void AnimateToState(double state) override;

  void ApplyValuesToDisplay();

  shell::CastDisplayConfigurator* const display_configurator_;

  const DisplaySettingsManager::ColorTemperatureConfig config_;

  float start_temperature_;
  float current_temperature_;
  float target_temperature_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_COLOR_TEMPERATURE_ANIMATION_H_
