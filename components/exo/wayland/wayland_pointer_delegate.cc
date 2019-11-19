// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_pointer_delegate.h"

#include <linux/input.h>
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
  struct {
    ui::EventFlags flag;
    uint32_t value;
  } buttons[] = {
      {ui::EF_LEFT_MOUSE_BUTTON, BTN_LEFT},
      {ui::EF_RIGHT_MOUSE_BUTTON, BTN_RIGHT},
      {ui::EF_MIDDLE_MOUSE_BUTTON, BTN_MIDDLE},
      {ui::EF_FORWARD_MOUSE_BUTTON, BTN_FORWARD},
      {ui::EF_BACK_MOUSE_BUTTON, BTN_BACK},
  };
  for (auto button : buttons) {
    if (button_flags & button.flag) {
      SendTimestamp(time_stamp);
      SerialTracker::EventType event_type =
          pressed ? SerialTracker::EventType::POINTER_BUTTON_DOWN
                  : SerialTracker::EventType::POINTER_BUTTON_UP;
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
  // Same as Weston, the reference compositor.
  const double kAxisStepDistance = 10.0 / ui::MouseWheelEvent::kWheelDelta;

  if (wl_resource_get_version(pointer_resource_) >=
      WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
    int32_t axis_source =
        discrete ? WL_POINTER_AXIS_SOURCE_WHEEL : WL_POINTER_AXIS_SOURCE_FINGER;
    wl_pointer_send_axis_source(pointer_resource_, axis_source);
  }

  double x_value = offset.x() * kAxisStepDistance;
  SendTimestamp(time_stamp);
  wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                       WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                       wl_fixed_from_double(-x_value));

  double y_value = offset.y() * kAxisStepDistance;
  SendTimestamp(time_stamp);
  wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                       WL_POINTER_AXIS_VERTICAL_SCROLL,
                       wl_fixed_from_double(-y_value));
}

void WaylandPointerDelegate::OnPointerScrollStop(base::TimeTicks time_stamp) {
  if (wl_resource_get_version(pointer_resource_) >=
      WL_POINTER_AXIS_STOP_SINCE_VERSION) {
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
