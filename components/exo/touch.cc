// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/touch.h"

#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/input_trace.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

gfx::PointF EventLocationInWindow(ui::TouchEvent* event, aura::Window* window) {
  ui::Layer* root = window->GetRootWindow()->layer();
  ui::Layer* target = window->layer();

  gfx::Transform transform;
  target->GetTargetTransformRelativeTo(root, &transform);
  gfx::PointF point = event->root_location_f();
  return transform.InverseMapPoint(point).value_or(point);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Touch, public:

Touch::Touch(TouchDelegate* delegate, Seat* seat)
    : delegate_(delegate), seat_(seat) {
  ash::Shell::Get()->AddShellObserver(this);
  for (aura::Window* root : ash::Shell::GetAllRootWindows()) {
    root->AddPreTargetHandler(this);
  }
}

Touch::~Touch() {
  ash::Shell::Get()->RemoveShellObserver(this);
  for (aura::Window* root : ash::Shell::GetAllRootWindows()) {
    root->RemovePreTargetHandler(this);
  }
  delegate_->OnTouchDestroying(this);
  if (HasStylusDelegate())
    stylus_delegate_->OnTouchDestroying(this);
  CancelAllTouches();
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
  if (seat_->was_shutdown() || event->handled()) {
    return;
  }

  // TODO(crbug.com/40061238): Investigate if we need to do something similar to
  // the filter in `Pointer::OnMouseEvent` when dragging. (not sending touch
  // events during drag)

  bool send_details = false;

  auto event_type = event->type();
  if ((event->flags() & ui::EF_RESERVED_FOR_GESTURE) != 0) {
    event_type = ui::EventType::kTouchCancelled;
  }

  const int touch_pointer_id = event->pointer_details().id;
  switch (event_type) {
    case ui::EventType::kTouchPressed: {
      // Early out if event doesn't contain a valid target for touch device.
      // TODO(b/147848270): Verify GetEffectiveTargetForEvent gets the correct
      // surface when input is captured.
      Surface* target = GetEffectiveTargetForEvent(event);
      if (!target)
        return;

      TRACE_EXO_INPUT_EVENT(event);
      DCHECK(touch_points_surface_map_.find(touch_pointer_id) ==
             touch_points_surface_map_.end());

      touch_points_surface_map_.emplace(touch_pointer_id, target);

      // Update the count of pointers on the target surface.
      auto it = surface_touch_count_map_.find(target);
      if (it == surface_touch_count_map_.end()) {
        target->AddSurfaceObserver(this);
        surface_touch_count_map_.emplace(target, 1);
      } else {
        it->second++;
      }

      // Convert location to target surface coordinate space.
      const gfx::PointF location =
          EventLocationInWindow(event, target->window());

      // Generate a touch down event for the target surface.
      delegate_->OnTouchDown(target, event->time_stamp(), touch_pointer_id,
                             location);
      if (stylus_delegate_ && event->pointer_details().pointer_type !=
                                  ui::EventPointerType::kTouch) {
        stylus_delegate_->OnTouchTool(touch_pointer_id,
                                      event->pointer_details().pointer_type);
      }
      send_details = true;
    } break;
    case ui::EventType::kTouchReleased: {
      auto it = touch_points_surface_map_.find(touch_pointer_id);
      if (it == touch_points_surface_map_.end())
        return;

      Surface* target = it->second;
      DCHECK(target);

      TRACE_EXO_INPUT_EVENT(event);

      touch_points_surface_map_.erase(it);

      // Update the count of pointers on the target surface.
      auto count_it = surface_touch_count_map_.find(target);
      if (count_it == surface_touch_count_map_.end())
        return;
      if ((--count_it->second) <= 0) {
        surface_touch_count_map_.erase(target);
        target->RemoveSurfaceObserver(this);
      }

      delegate_->OnTouchUp(event->time_stamp(), touch_pointer_id);
      seat_->AbortPendingDragOperation();
    } break;
    case ui::EventType::kTouchMoved: {
      auto it = touch_points_surface_map_.find(touch_pointer_id);
      if (it == touch_points_surface_map_.end())
        return;

      Surface* target = it->second;
      DCHECK(target);

      TRACE_EXO_INPUT_EVENT(event);

      // Convert location to focus surface coordinate space.
      gfx::PointF location = EventLocationInWindow(event, target->window());
      delegate_->OnTouchMotion(event->time_stamp(), touch_pointer_id, location);
      send_details = true;
    } break;
    case ui::EventType::kTouchCancelled: {
      TRACE_EXO_INPUT_EVENT(event);
      // Cancel the full set of touch sequences as soon as one is canceled.
      CancelAllTouches();
      delegate_->OnTouchCancel();

      seat_->AbortPendingDragOperation();
    } break;
    default:
      NOTREACHED_IN_MIGRATION();
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

    if (stylus_delegate_ &&
        event->pointer_details().pointer_type != ui::EventPointerType::kTouch) {
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
  // TODO(b/147848407): Do not cancel touches on surfaces of different clients
  // when this surface dies.
  CancelAllTouches();
  delegate_->OnTouchCancel();
}

// ash::ShellObserver:
void Touch::OnRootWindowAdded(aura::Window* root_window) {
  root_window->AddPreTargetHandler(this);
}

void Touch::OnRootWindowWillShutdown(aura::Window* root_window) {
  root_window->RemovePreTargetHandler(this);
}

////////////////////////////////////////////////////////////////////////////////
// Touch, private:

Surface* Touch::GetEffectiveTargetForEvent(ui::LocatedEvent* event) const {
  Surface* target = GetTargetSurfaceForLocatedEvent(event);

  if (!target)
    return nullptr;

  return delegate_->CanAcceptTouchEventsForSurface(target) ? target : nullptr;
}

void Touch::CancelAllTouches() {
  base::ranges::for_each(surface_touch_count_map_, [this](auto& it) {
    it.first->RemoveSurfaceObserver(this);
  });
  touch_points_surface_map_.clear();
  surface_touch_count_map_.clear();
}

}  // namespace exo
