// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_CONTROLLER_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_CONTROLLER_MODEL_H_

#include "base/time/time.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// The ControllerModel encapsulates generic controller information read from the
// platform-specific VR subsystem (e.g., GVR). It is used by both the
// UiInputManager (for generating gestures), and by the UI for rendering the
// controller.
struct VR_BASE_EXPORT ControllerModel {
  enum ButtonState {
    kUp,
    kDown,
  };

  enum Handedness {
    kRightHanded,
    kLeftHanded,
  };

  ControllerModel();
  ControllerModel(const ControllerModel& other);
  ~ControllerModel();

  gfx::Transform transform;
  gfx::Vector3dF laser_direction;
  gfx::Point3F laser_origin;
  ButtonState touchpad_button_state = kUp;
  ButtonState app_button_state = kUp;
  ButtonState home_button_state = kUp;
  bool touching_touchpad = false;
  gfx::PointF touchpad_touch_position;
  float opacity = 1.0f;
  bool resting_in_viewport = false;
  bool recentered = false;
  Handedness handedness = kRightHanded;
  base::TimeTicks last_orientation_timestamp;
  base::TimeTicks last_button_timestamp;
  int battery_level = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_CONTROLLER_MODEL_H_
