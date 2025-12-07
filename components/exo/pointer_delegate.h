// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_DELEGATE_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"

namespace gfx {
class PointF;
class Vector2dF;
}

namespace exo {
class Pointer;
class Surface;

// Handles events on pointers in context-specific ways.
class PointerDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  // This should return true if |surface| is a valid target for this pointer.
  // E.g. the surface is owned by the same client as the pointer.
  virtual bool CanAcceptPointerEventsForSurface(Surface* surface) const = 0;

  // Called when pointer enters a new valid target surface. |location|
  // is the location of pointer relative to the origin of surface and
  // |button_flags| contains all currently pressed buttons.
  virtual void OnPointerEnter(Surface* surface,
                              const gfx::PointF& location,
                              int pressed_button_flags) = 0;

  // Called when pointer leaves a valid target surface.
  virtual void OnPointerLeave(Surface* surface) = 0;

  // Called when pointer moved within the current target surface.
  virtual void OnPointerMotion(base::TimeTicks time_stamp,
                               const gfx::PointF& location) = 0;

  // Called when pointer button state changed. |changed_button_flags| contains
  // all buttons that changed. |pressed| is true if buttons entered pressed
  // state.
  virtual void OnPointerButton(base::TimeTicks time_stamp,
                               int changed_button_flags,
                               bool pressed) = 0;

  // Called when pointer is scrolling. |offset| contains the direction and
  // distance of the change. |discrete| is true if the scrolling is caused
  // by a discrete device such as a scroll wheel.
  virtual void OnPointerScroll(base::TimeTicks time_stamp,
                               const gfx::Vector2dF& offset,
                               bool discrete) = 0;

  // Called to end a sequence of finger (continuous) scroll events, e.g.
  // lifting the fingers from the touchpad after scrolling.
  virtual void OnFingerScrollStop(base::TimeTicks time_stamp) = 0;

  // Called after all pointer information of this frame has been set and the
  // client should evaluate the updated state. No events are being sent before
  // this method is called.
  virtual void OnPointerFrame() = 0;

 protected:
  virtual ~PointerDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_DELEGATE_H_
