// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_PLATFORM_CONTROLLER_H_
#define CHROME_BROWSER_VR_PLATFORM_CONTROLLER_H_

#include "base/time/time.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/vr_export.h"

namespace gfx {
class PointF;
}

namespace vr {

// This class is not platform-specific.  It will be backed by platform-specific
// controller code, but its interface must be platform-agnostic. For example,
// the enumeration of buttons may map to buttons with different names on a
// different platform's controller, but the functionality must exist. I.e., the
// concept of "the button you press to exit fullscreen / presentation" is
// universal.
class VR_EXPORT PlatformController {
 public:
  enum ButtonType {
    kButtonHome,
    kButtonTypeFirst = kButtonHome,
    kButtonMenu,
    kButtonSelect,
    kButtonTypeNumber,
  };

  virtual ~PlatformController() {}

  virtual bool IsButtonDown(ButtonType type) const = 0;
  virtual bool ButtonUpHappened(ButtonType type) const = 0;
  virtual bool ButtonDownHappened(ButtonType type) const = 0;
  virtual bool IsTouchingTrackpad() const = 0;
  virtual gfx::PointF GetPositionInTrackpad() const = 0;
  virtual base::TimeTicks GetLastOrientationTimestamp() const = 0;
  virtual base::TimeTicks GetLastTouchTimestamp() const = 0;
  virtual base::TimeTicks GetLastButtonTimestamp() const = 0;
  virtual ControllerModel::Handedness GetHandedness() const = 0;
  virtual bool GetRecentered() const = 0;
  virtual int GetBatteryLevel() const = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_PLATFORM_CONTROLLER_H_
