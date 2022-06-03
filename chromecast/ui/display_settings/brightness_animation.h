// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_BRIGHTNESS_ANIMATION_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_BRIGHTNESS_ANIMATION_H_

#include "chromecast/ui/display_settings_manager.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"

namespace chromecast {

// Defines a linear animation type to animate the color temperature between two
// values in a given time duration.
class BrightnessAnimation : public gfx::LinearAnimation {
 public:
  explicit BrightnessAnimation(DisplaySettingsManager::Delegate* controller);
  BrightnessAnimation(const BrightnessAnimation&) = delete;
  BrightnessAnimation& operator=(const BrightnessAnimation&) = delete;
  ~BrightnessAnimation() override;

  // Starts a new brightness animation from the |current_brightness_| to the
  // given |new_target_brightness| in the given |duration|.
  void AnimateToNewValue(float new_target_brightness, base::TimeDelta duration);

 private:
  // gfx::LinearAnimation implementation:
  void AnimateToState(double state) override;

  void ApplyValuesToDisplay();

  DisplaySettingsManager::Delegate* const controller_;

  float start_brightness_;
  float current_brightness_;
  float target_brightness_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_BRIGHTNESS_ANIMATION_H_
