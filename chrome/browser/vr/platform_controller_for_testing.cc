// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/platform_controller_for_testing.h"
#include "base/time/time.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

PlatformControllerForTesting::PlatformControllerForTesting(
    ControllerModel* prev_model,
    ControllerModel* cur_model,
    base::TimeTicks last_touchpad_timestamp)
    : prev_model_(prev_model),
      cur_model_(cur_model),
      last_touchpad_timestamp_(last_touchpad_timestamp) {}

bool PlatformControllerForTesting::IsButtonDown(
    PlatformController::ButtonType type) const {
  switch (type) {
    case PlatformController::ButtonType::kButtonMenu:
      return cur_model_->app_button_state ==
             ControllerModel::ButtonState::kDown;
    case PlatformController::ButtonType::kButtonSelect:
      return cur_model_->touchpad_button_state ==
             ControllerModel::ButtonState::kDown;
    default:
      return false;
  }
}

bool PlatformControllerForTesting::ButtonUpHappened(
    PlatformController::ButtonType type) const {
  switch (type) {
    case PlatformController::ButtonType::kButtonMenu:
      return (cur_model_->app_button_state ==
                  ControllerModel::ButtonState::kUp &&
              cur_model_->app_button_state != prev_model_->app_button_state);
    case PlatformController::ButtonType::kButtonSelect:
      return (cur_model_->touchpad_button_state ==
                  ControllerModel::ButtonState::kUp &&
              cur_model_->touchpad_button_state !=
                  prev_model_->touchpad_button_state);
    default:
      return false;
  }
}

bool PlatformControllerForTesting::ButtonDownHappened(
    PlatformController::ButtonType type) const {
  switch (type) {
    case PlatformController::ButtonType::kButtonMenu:
      return (cur_model_->app_button_state ==
                  ControllerModel::ButtonState::kDown &&
              cur_model_->app_button_state != prev_model_->app_button_state);
    case PlatformController::ButtonType::kButtonSelect:
      return (cur_model_->touchpad_button_state ==
                  ControllerModel::ButtonState::kDown &&
              cur_model_->touchpad_button_state !=
                  prev_model_->touchpad_button_state);
    default:
      return false;
  }
}

bool PlatformControllerForTesting::IsTouchingTrackpad() const {
  return cur_model_->touching_touchpad;
}

gfx::PointF PlatformControllerForTesting::GetPositionInTrackpad() const {
  return cur_model_->touchpad_touch_position;
}

base::TimeTicks PlatformControllerForTesting::GetLastOrientationTimestamp()
    const {
  return cur_model_->last_orientation_timestamp;
}

base::TimeTicks PlatformControllerForTesting::GetLastTouchTimestamp() const {
  return last_touchpad_timestamp_;
}

base::TimeTicks PlatformControllerForTesting::GetLastButtonTimestamp() const {
  return cur_model_->last_button_timestamp;
}

ControllerModel::Handedness PlatformControllerForTesting::GetHandedness()
    const {
  return ControllerModel::Handedness::kRightHanded;
}

bool PlatformControllerForTesting::GetRecentered() const {
  return false;
}

int PlatformControllerForTesting::GetBatteryLevel() const {
  return 100;
}

}  // namespace vr
