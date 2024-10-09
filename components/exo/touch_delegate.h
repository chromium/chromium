// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOUCH_DELEGATE_H_
#define COMPONENTS_EXO_TOUCH_DELEGATE_H_

#include "base/time/time.h"

namespace gfx {
class PointF;
}

namespace exo {
class Surface;
class Touch;

// Handles events on touch devices in context-specific ways.
class TouchDelegate {
 public:
  // Called at the top of the touch device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnTouchDestroying(Touch* touch) = 0;

  // This should return true if |surface| is a valid target for this touch
  // device. E.g. the surface is owned by the same client as the touch device.
  virtual bool CanAcceptTouchEventsForSurface(Surface* surface) const = 0;

  // Called when a new touch point has appeared on the surface. This touch
  // point is assigned a unique ID. Future events from this touch point
  // reference this ID. |location| is the initial location of touch point
  // relative to the origin of the surface.
  virtual void OnTouchDown(Surface* surface,
                           base::TimeTicks time_stamp,
                           int id,
                           const gfx::PointF& location) = 0;

  // Called when a touch point has disappeared. No further events will be sent
  // for this touch point.
  virtual void OnTouchUp(base::TimeTicks time_stamp, int id) = 0;

  // Called when a touch point has changed coordinates.
  virtual void OnTouchMotion(base::TimeTicks time_stamp,
                             int id,
                             const gfx::PointF& location) = 0;

  // Called when a touch point has changed its shape.
  virtual void OnTouchShape(int id, float major, float minor) = 0;

  // Called when the client should apply all updated touches.
  virtual void OnTouchFrame() = 0;

  // Called when the touch session has been canceled. Touch cancellation
  // applies to all touch points currently active.
  virtual void OnTouchCancel() = 0;

 protected:
  virtual ~TouchDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOUCH_DELEGATE_H_
