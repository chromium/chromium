// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_touch_delegate.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "components/exo/touch.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

WaylandTouchDelegate::WaylandTouchDelegate(wl_resource* touch_resource,
                                           SerialTracker* serial_tracker)
    : touch_resource_(touch_resource), serial_tracker_(serial_tracker) {}

void WaylandTouchDelegate::OnTouchDestroying(Touch* touch) {
  delete this;
}

bool WaylandTouchDelegate::CanAcceptTouchEventsForSurface(
    Surface* surface) const {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  // We can accept events for this surface if the client is the same as the
  // touch resource.
  return surface_resource &&
         wl_resource_get_client(surface_resource) == client();
}
void WaylandTouchDelegate::OnTouchDown(Surface* surface,
                                       base::TimeTicks time_stamp,
                                       int id,
                                       const gfx::PointF& location) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  SendTimestamp(time_stamp);
  wl_touch_send_down(
      touch_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::TOUCH_DOWN),
      TimeTicksToMilliseconds(time_stamp), surface_resource, id,
      wl_fixed_from_double(location.x()), wl_fixed_from_double(location.y()));
}
void WaylandTouchDelegate::OnTouchUp(base::TimeTicks time_stamp, int id) {
  SendTimestamp(time_stamp);
  wl_touch_send_up(
      touch_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::TOUCH_UP),
      TimeTicksToMilliseconds(time_stamp), id);
}
void WaylandTouchDelegate::OnTouchMotion(base::TimeTicks time_stamp,
                                         int id,
                                         const gfx::PointF& location) {
  SendTimestamp(time_stamp);
  wl_touch_send_motion(touch_resource_, TimeTicksToMilliseconds(time_stamp), id,
                       wl_fixed_from_double(location.x()),
                       wl_fixed_from_double(location.y()));
}
void WaylandTouchDelegate::OnTouchShape(int id, float major, float minor) {
  if (wl_resource_get_version(touch_resource_) >=
      WL_TOUCH_SHAPE_SINCE_VERSION) {
    wl_touch_send_shape(touch_resource_, id, wl_fixed_from_double(major),
                        wl_fixed_from_double(minor));
  }
}
void WaylandTouchDelegate::OnTouchFrame() {
  if (wl_resource_get_version(touch_resource_) >=
      WL_TOUCH_FRAME_SINCE_VERSION) {
    wl_touch_send_frame(touch_resource_);
  }
  wl_client_flush(client());
}
void WaylandTouchDelegate::OnTouchCancel() {
  wl_touch_send_cancel(touch_resource_);
}

wl_client* WaylandTouchDelegate::client() const {
  return wl_resource_get_client(touch_resource_);
}

}  // namespace wayland
}  // namespace exo
