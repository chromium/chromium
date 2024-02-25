// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/color_temperature_animation.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/time/time.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if defined(USE_AURA)
#include "chromecast/browser/cast_display_configurator.h"
#endif  // defined(USE_AURA)

namespace chromecast {

namespace {

constexpr base::TimeDelta kManualAnimationDuration = base::Seconds(1);

const int kAnimationFrameRate = 30;

float Interpolate(const std::vector<float>& vec, float idx) {
  size_t i = idx;
  if (i == idx)
    return vec[i];
  float frac = idx - i;
  return frac * vec[i + 1] + (1 - frac) * vec[i];
}

}  // namespace

ColorTemperatureAnimation::ColorTemperatureAnimation(
    shell::CastDisplayConfigurator* display_configurator,
    const DisplaySettingsManager::ColorTemperatureConfig& config)
    : gfx::LinearAnimation(kManualAnimationDuration,
                           kAnimationFrameRate,
                           nullptr),
      display_configurator_(display_configurator),
      config_(config),
      start_temperature_(config.neutral_temperature),
      current_temperature_(config.neutral_temperature),
      target_temperature_(config_.neutral_temperature) {
#if defined(USE_AURA)
  DCHECK(display_configurator_);
#endif  // defined(USE_AURA)
  ApplyValuesToDisplay();
}

ColorTemperatureAnimation::~ColorTemperatureAnimation() = default;

void ColorTemperatureAnimation::AnimateToNewValue(float new_target_temperature,
                                                  base::TimeDelta duration) {
  start_temperature_ = current_temperature_;
  target_temperature_ = std::clamp(new_target_temperature, 1000.0f, 20000.0f);

  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    // Animations are disabled. Apply the target temperature directly to the
    // compositor.
    current_temperature_ = target_temperature_;
    ApplyValuesToDisplay();
    Stop();
    return;
  }

  // This will reset the animation timer to the beginning.
  SetDuration(duration);
  Start();
}

void ColorTemperatureAnimation::AnimateToNeutral(base::TimeDelta duration) {
  AnimateToNewValue(config_.neutral_temperature, duration);
}

void ColorTemperatureAnimation::AnimateToState(double state) {
  state = std::clamp(state, 0.0, 1.0);
  current_temperature_ =
      start_temperature_ + (target_temperature_ - start_temperature_) * state;
  ApplyValuesToDisplay();
}

void ColorTemperatureAnimation::ApplyValuesToDisplay() {
  // Clamp temperature value to table range.
  float kelvin =
      std::clamp(current_temperature_, config_.temperature_values.front(),
                 config_.temperature_values.back());
  size_t i = 0;
  // Find greatest index whose value is <= |kelvin|. This is safe since |kelvin|
  // is clamped to fall within the table range.
  while (kelvin > config_.temperature_values[i + 1])
    ++i;

  // Backwards interpolate the index from the temperature table.
  float i_interp = i + (kelvin - config_.temperature_values[i]) /
                           (config_.temperature_values[i + 1] -
                            config_.temperature_values[i]);
  float red_scale =
      Interpolate(config_.red_values, i_interp) / config_.full_color;
  float green_scale =
      Interpolate(config_.green_values, i_interp) / config_.full_color;
  float blue_scale =
      Interpolate(config_.blue_values, i_interp) / config_.full_color;

  DVLOG(2) << "RGB scaling: {" << red_scale << ", " << green_scale << ", "
           << blue_scale << "}";
  if (display_configurator_) {
#if defined(USE_AURA)
    DVLOG(1) << "Color temperature set to " << kelvin << " kelvin.";
    display::ColorTemperatureAdjustment cta;
    cta.srgb_matrix.vals[0][0] = red_scale;
    cta.srgb_matrix.vals[1][1] = green_scale;
    cta.srgb_matrix.vals[2][2] = blue_scale;
    display_configurator_->SetColorTemperatureAdjustment(cta);
#endif  // defined(USE_AURA)
  }
}

}  // namespace chromecast
