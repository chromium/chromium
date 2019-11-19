// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/touch.h"

#include "components/exo/input_trace.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// Helper function that returns an iterator to the first item in |vector|
// with |value|.
template <typename T, typename U>
typename T::iterator FindVectorItem(T& vector, U value) {
  return std::find(vector.begin(), vector.end(), value);
}

// Helper function that returns true if |vector| contains an item with |value|.
template <typename T, typename U>
bool VectorContainsItem(T& vector, U value) {
  return FindVectorItem(vector, value) != vector.end();
}

gfx::PointF EventLocationInWindow(ui::TouchEvent* event, aura::Window* window) {
  ui::Layer* root = window->GetRootWindow()->layer();
  ui::Layer* target = window->layer();

  gfx::Transform transform;
  target->GetTargetTransformRelativeTo(root, &transform);
  auto point = gfx::Point3F(event->root_location_f());
  transform.TransformPointReverse(&point);
  return point.AsPointF();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Touch, public:

Touch::Touch(TouchDelegate* delegate, Seat* seat)
    : delegate_(delegate), seat_(seat) {
  WMHelper::GetInstance()->AddPreTargetHandler(this);
}

Touch::~Touch() {
  delegate_->OnTouchDestroying(this);
  if (HasStylusDelegate())
    stylus_delegate_->OnTouchDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
}

void Touch::SetStylusDelegate(TouchStylusDelegate* delegate) {
  stylus_delegate_ = delegate;
}

bool Touch::HasStylusDelegate() const {
  return !!stylus_delegate_;
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Touch::OnTouchEvent(ui::TouchEvent* event) {
  bool send_details = false;

  const int touch_pointer_id = event->pointer_details().id;
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED: {
      // Early out if event doesn't contain a valid target for touch device.
      Surface* target = GetEffectiveTargetForEvent(event);
      if (!target)
        return;

      TRACE_EXO_INPUT_EVENT(event);

      // If this is the first touch point then target becomes the focus surface
      // until all touch points have been released.
      if (touch_points_.empty()) {
        DCHECK(!focus_);
        focus_ = target;
        focus_->AddSurfaceObserver(this);
      }

      DCHECK(!VectorContainsItem(touch_points_, touch_pointer_id));
      touch_points_.push_back(touch_pointer_id);

      // Convert location to focus surface coordinate space.
      DCHECK(focus_);
      gfx::PointF location = EventLocationInWindow(event, focus_->window());

      // Generate a touch down event for the focus surface. Note that this can
      // be different from the target surface.
      delegate_->OnTouchDown(focus_, event->time_stamp(), touch_pointer_id,
                             location);
      if (stylus_delegate_ && event->pointer_details().pointer_type !=
                                  ui::EventPointerType::POINTER_TYPE_TOUCH) {
        stylus_delegate_->OnTouchTool(touch_pointer_id,
                                      event->pointer_details().pointer_type);
      }
      send_details = true;
    } break;
    case ui::ET_TOUCH_RELEASED: {
      auto it = FindVectorItem(touch_points_, touch_pointer_id);
      if (it == touch_points_.end())
        return;

      TRACE_EXO_INPUT_EVENT(event);

      touch_points_.erase(it);

      // Reset focus surface if this is the last touch point.
      if (touch_points_.empty()) {
        DCHECK(focus_);
        focus_->RemoveSurfaceObserver(this);
        focus_ = nullptr;
      }

      delegate_->OnTouchUp(event->time_stamp(), touch_pointer_id);
      seat_->AbortPendingDragOperation();
    } break;
    case ui::ET_TOUCH_MOVED: {
      auto it = FindVectorItem(touch_points_, touch_pointer_id);
      if (it == touch_points_.end())
        return;

      TRACE_EXO_INPUT_EVENT(event);

      DCHECK(focus_);
      // Convert location to focus surface coordinate space.
      gfx::PointF location = EventLocationInWindow(event, focus_->window());
      delegate_->OnTouchMotion(event->time_stamp(), touch_pointer_id, location);
      send_details = true;
    } break;
    case ui::ET_TOUCH_CANCELLED: {
      auto it = FindVectorItem(touch_points_, touch_pointer_id);
      if (it == touch_points_.end())
        return;

      TRACE_EXO_INPUT_EVENT(event);

      DCHECK(focus_);
      focus_->RemoveSurfaceObserver(this);
      focus_ = nullptr;

      // Cancel the full set of touch sequences as soon as one is canceled.
      touch_points_.clear();
      delegate_->OnTouchCancel();
      seat_->AbortPendingDragOperation();
    } break;
    default:
      NOTREACHED();
      return;
  }
  if (send_details) {
    // Some devices do not report radius_y/minor. We assume a circular shape
    // in that case.
    float major = event->pointer_details().radius_x * 2.0f;
    float minor = event->pointer_details().radius_y * 2.0f;
    if (!minor)
      minor = major;
    delegate_->OnTouchShape(touch_pointer_id, major, minor);

    if (stylus_delegate_ && event->pointer_details().pointer_type !=
                                ui::EventPointerType::POINTER_TYPE_TOUCH) {
      if (!std::isnan(event->pointer_details().force)) {
        stylus_delegate_->OnTouchForce(event->time_stamp(), touch_pointer_id,
                                       event->pointer_details().force);
      }
      stylus_delegate_->OnTouchTilt(
          event->time_stamp(), touch_pointer_id,
          gfx::Vector2dF(event->pointer_details().tilt_x,
                         event->pointer_details().tilt_y));
    }
  }
  // TODO(denniskempin): Extend ui::TouchEvent to signal end of sequence of
  // touch events to send TouchFrame once after all touches have been updated.
  delegate_->OnTouchFrame();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Touch::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  focus_ = nullptr;
  surface->RemoveSurfaceObserver(this);

  // Cancel touch sequences.
  DCHECK_NE(touch_points_.size(), 0u);
  touch_points_.clear();
  delegate_->OnTouchCancel();
}

////////////////////////////////////////////////////////////////////////////////
// Touch, private:

Surface* Touch::GetEffectiveTargetForEvent(ui::LocatedEvent* event) const {
  Surface* target = GetTargetSurfaceForLocatedEvent(event);

  if (!target)
    return nullptr;

  return delegate_->CanAcceptTouchEventsForSurface(target) ? target : nullptr;
}

}  // namespace exo
