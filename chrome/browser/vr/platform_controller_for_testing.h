// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_PLATFORM_CONTROLLER_FOR_TESTING_H_
#define CHROME_BROWSER_VR_PLATFORM_CONTROLLER_FOR_TESTING_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/platform_controller.h"

namespace vr {

class PlatformControllerForTesting : public PlatformController {
 public:
  explicit PlatformControllerForTesting(
      ControllerModel* prev_model,
      ControllerModel* cur_model,
      base::TimeTicks last_touchpad_timestamp);
  ~PlatformControllerForTesting() override {}

  bool IsButtonDown(PlatformController::ButtonType type) const override;
  bool ButtonUpHappened(PlatformController::ButtonType type) const override;
  bool ButtonDownHappened(PlatformController::ButtonType type) const override;
  bool IsTouchingTrackpad() const override;
  gfx::PointF GetPositionInTrackpad() const override;
  base::TimeTicks GetLastOrientationTimestamp() const override;
  base::TimeTicks GetLastTouchTimestamp() const override;
  base::TimeTicks GetLastButtonTimestamp() const override;
  ControllerModel::Handedness GetHandedness() const override;
  bool GetRecentered() const override;
  int GetBatteryLevel() const override;

 private:
  ControllerModel* prev_model_;
  ControllerModel* cur_model_;
  base::TimeTicks last_touchpad_timestamp_;
  DISALLOW_COPY_AND_ASSIGN(PlatformControllerForTesting);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_PLATFORM_CONTROLLER_FOR_TESTING_H_
