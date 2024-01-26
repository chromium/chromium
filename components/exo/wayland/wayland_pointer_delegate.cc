// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_pointer_delegate.h"

#include <linux/input-event-codes.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "components/exo/pointer.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace exo {
namespace wayland {

WaylandPointerDelegate::WaylandPointerDelegate(wl_resource* pointer_resource,
                                               SerialTracker* serial_tracker)
    : pointer_resource_(pointer_resource), serial_tracker_(serial_tracker) {}

void WaylandPointerDelegate::OnPointerDestroying(Pointer* pointer) {
  delete this;
}

bool WaylandPointerDelegate::CanAcceptPointerEventsForSurface(
    Surface* surface) const {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  // We can accept events for this surface if the client is the same as the
  // pointer.
  return surface_resource &&
         wl_resource_get_client(surface_resource) == client();
}

void WaylandPointerDelegate::OnPointerEnter(Surface* surface,
                                            const gfx::PointF& location,
                                            int button_flags) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  // Should we be sending button events to the client before the enter event
  // if client's pressed button state is different from |button_flags|?
  wl_pointer_send_enter(
      pointer_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::POINTER_ENTER),
      surface_resource, wl_fixed_from_double(location.x()),
      wl_fixed_from_double(location.y()));
}

void WaylandPointerDelegate::OnPointerLeave(Surface* surface) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  wl_pointer_send_leave(
      pointer_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::POINTER_LEAVE),
      surface_resource);
}

void WaylandPointerDelegate::OnPointerMotion(base::TimeTicks time_stamp,
                                             const gfx::PointF& location) {
  SendTimestamp(time_stamp);
  wl_pointer_send_motion(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                         wl_fixed_from_double(location.x()),
                         wl_fixed_from_double(location.y()));
}

void WaylandPointerDelegate::OnPointerButton(base::TimeTicks time_stamp,
                                             int button_flags,
                                             bool pressed) {
  constexpr struct {
    ui::EventFlags flag;
    uint32_t value;
    SerialTracker::EventType down;
    SerialTracker::EventType up;
  } buttons[] = {
      {ui::EF_LEFT_MOUSE_BUTTON, BTN_LEFT,
       SerialTracker::EventType::POINTER_LEFT_BUTTON_DOWN,
       SerialTracker::EventType::POINTER_LEFT_BUTTON_UP},
      {ui::EF_RIGHT_MOUSE_BUTTON, BTN_RIGHT,
       SerialTracker::EventType::POINTER_RIGHT_BUTTON_DOWN,
       SerialTracker::EventType::POINTER_RIGHT_BUTTON_UP},
      {ui::EF_MIDDLE_MOUSE_BUTTON, BTN_MIDDLE,
       SerialTracker::EventType::POINTER_MIDDLE_BUTTON_DOWN,
       SerialTracker::EventType::POINTER_MIDDLE_BUTTON_UP},
      {ui::EF_FORWARD_MOUSE_BUTTON, BTN_EXTRA,
       SerialTracker::EventType::POINTER_FORWARD_BUTTON_DOWN,
       SerialTracker::EventType::POINTER_FORWARD_BUTTON_UP},
      {ui::EF_BACK_MOUSE_BUTTON, BTN_SIDE,
       SerialTracker::EventType::POINTER_BACK_BUTTON_DOWN,
       SerialTracker::EventType::POINTER_BACK_BUTTON_UP},
  };
  for (auto button : buttons) {
    if (button_flags & button.flag) {
      SendTimestamp(time_stamp);
      const SerialTracker::EventType event_type =
          (pressed ? button.down : button.up);
      wl_pointer_send_button(pointer_resource_,
                             serial_tracker_->GetNextSerial(event_type),
                             TimeTicksToMilliseconds(time_stamp), button.value,
                             pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                     : WL_POINTER_BUTTON_STATE_RELEASED);
    }
  }
}

void WaylandPointerDelegate::OnPointerScroll(base::TimeTicks time_stamp,
                                             const gfx::Vector2dF& offset,
                                             bool discrete) {
  // The unit aura considers to be "one scroll tick".
  const int kAuraScrollUnit = ui::MouseWheelEvent::kWheelDelta;

  // Weston, the reference compositor, treats one scroll tick as 10 units, with
  // no acceleration applied.
  constexpr int kWaylandScrollUnit = 10;

  // The ratio between the wayland and aura unit sizes. Multiplying by this
  // converts from aura units to wayland units, dividing does the reverse.
  const double kAxisStepDistance = static_cast<double>(kWaylandScrollUnit) /
                                   static_cast<double>(kAuraScrollUnit);

  if (wl_resource_get_version(pointer_resource_) >=
      WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
    int32_t axis_source =
        discrete ? WL_POINTER_AXIS_SOURCE_WHEEL : WL_POINTER_AXIS_SOURCE_FINGER;
    wl_pointer_send_axis_source(pointer_resource_, axis_source);
  }

  double x_value = -offset.x() * kAxisStepDistance;
  double y_value = -offset.y() * kAxisStepDistance;

  // ::axis_discrete events must be sent before their corresponding ::axis
  // events, per the specification.
  if (wl_resource_get_version(pointer_resource_) >=
          WL_POINTER_AXIS_DISCRETE_SINCE_VERSION &&
      discrete) {
    // Ensure that we never round the discrete value down to 0.
    int discrete_x = static_cast<int>(x_value / kWaylandScrollUnit);
    if (discrete_x == 0 && x_value != 0) {
      discrete_x = copysign(1, x_value);
    }
    int discrete_y = static_cast<int>(y_value / kWaylandScrollUnit);
    if (discrete_y == 0 && y_value != 0) {
      discrete_y = copysign(1, y_value);
    }

    wl_pointer_send_axis_discrete(
        pointer_resource_, WL_POINTER_AXIS_HORIZONTAL_SCROLL, discrete_x);
    wl_pointer_send_axis_discrete(pointer_resource_,
                                  WL_POINTER_AXIS_VERTICAL_SCROLL, discrete_y);
  }

  SendTimestamp(time_stamp);
  wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                       WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                       wl_fixed_from_double(x_value));

  SendTimestamp(time_stamp);
  wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                       WL_POINTER_AXIS_VERTICAL_SCROLL,
                       wl_fixed_from_double(y_value));
}

void WaylandPointerDelegate::OnFingerScrollStop(base::TimeTicks time_stamp) {
  if (wl_resource_get_version(pointer_resource_) >=
      WL_POINTER_AXIS_STOP_SINCE_VERSION) {
    wl_pointer_send_axis_source(pointer_resource_,
                                WL_POINTER_AXIS_SOURCE_FINGER);
    SendTimestamp(time_stamp);
    wl_pointer_send_axis_stop(pointer_resource_,
                              TimeTicksToMilliseconds(time_stamp),
                              WL_POINTER_AXIS_HORIZONTAL_SCROLL);
    SendTimestamp(time_stamp);
    wl_pointer_send_axis_stop(pointer_resource_,
                              TimeTicksToMilliseconds(time_stamp),
                              WL_POINTER_AXIS_VERTICAL_SCROLL);
  }
}

void WaylandPointerDelegate::OnPointerFrame() {
  if (wl_resource_get_version(pointer_resource_) >=
      WL_POINTER_FRAME_SINCE_VERSION) {
    wl_pointer_send_frame(pointer_resource_);
  }
  wl_client_flush(client());
}

wl_client* WaylandPointerDelegate::client() const {
  return wl_resource_get_client(pointer_resource_);
}

}  // namespace wayland
}  // namespace exo
