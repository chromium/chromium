// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOUCH_STYLUS_DELEGATE_H_
#define COMPONENTS_EXO_TOUCH_STYLUS_DELEGATE_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"

namespace gfx {
class Vector2dF;
}

namespace exo {
class Touch;

// Handles stylus specific events as an extension to TouchDelegate.
class TouchStylusDelegate {
 public:
  // Called at the top of the touch's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnTouchDestroying(Touch* pointer) = 0;

  // Called to set the tool type of a touch. Only called once with the down
  // event. A tool type cannot change afterwards.
  virtual void OnTouchTool(int touch_id, ui::EventPointerType type) = 0;

  // Called when the force (pressure) of the touch changes.
  // Normalized to be [0, 1].
  virtual void OnTouchForce(base::TimeTicks time_stamp,
                            int touch_id,
                            float force) = 0;

  // Called when the tilt of a pen/stylus changes. Measured from surface normal
  // as plane angle in degrees, values lie in [-90,90]. A positive x is to the
  // right and a positive y is towards the user.
  virtual void OnTouchTilt(base::TimeTicks time_stamp,
                           int touch_id,
                           const gfx::Vector2dF& tilt) = 0;

 protected:
  virtual ~TouchStylusDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOUCH_STYLUS_DELEGATE_H_
