// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/brightness_animation.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/time/time.h"

namespace chromecast {

namespace {

constexpr base::TimeDelta kManualAnimationDuration = base::Seconds(0);

// Brightness changes are smoothed linearly over a 50 ms interval by the
// backlight controller IC.
const int kAnimationFrameRate = 20;

}  // namespace

BrightnessAnimation::BrightnessAnimation(
    DisplaySettingsManager::Delegate* controller)
    : gfx::LinearAnimation(kManualAnimationDuration,
                           kAnimationFrameRate,
                           nullptr),
      controller_(controller) {
  DCHECK(controller_);

  start_brightness_ = controller_->GetDisplayBrightness();
  current_brightness_ = start_brightness_;
  target_brightness_ = start_brightness_;
}

BrightnessAnimation::~BrightnessAnimation() {
  End();
}

void BrightnessAnimation::AnimateToNewValue(float new_target_brightness,
                                            base::TimeDelta duration) {
  start_brightness_ = controller_->GetDisplayBrightness();
  target_brightness_ = std::clamp(new_target_brightness, 0.0f, 1.0f);
  DVLOG(4) << "Animating to new_target_brightness " << new_target_brightness
           << " from current_brightness_=" << current_brightness_;

  if (start_brightness_ < 0.0f) {
    LOG(WARNING)
        << "Brightness animation started from invalid start_brightness_="
        << start_brightness_;
    start_brightness_ = target_brightness_;
    SetDuration(base::Seconds(0));
  } else {
    // This will reset the animation timer to the beginning.
    SetDuration(duration);
  }
  Start();
}

void BrightnessAnimation::AnimateToState(double state) {
  state = std::clamp(state, 0.0, 1.0);
  current_brightness_ =
      start_brightness_ + (target_brightness_ - start_brightness_) * state;
  ApplyValuesToDisplay();
}

void BrightnessAnimation::ApplyValuesToDisplay() {
  controller_->SetDisplayBrightness(current_brightness_, false);
}

}  // namespace chromecast
