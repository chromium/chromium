// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_pointer_gestures.h"

#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_swipe_v1 interface:

void pointer_gestures_get_swipe_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_pinch_v1 interface:

class WaylandPointerGesturePinchDelegate : public PointerGesturePinchDelegate {
 public:
  WaylandPointerGesturePinchDelegate(wl_resource* resource, Pointer* pointer)
      : resource_(resource), pointer_(pointer) {
    pointer_->SetGesturePinchDelegate(this);
  }

  WaylandPointerGesturePinchDelegate(
      const WaylandPointerGesturePinchDelegate&) = delete;
  WaylandPointerGesturePinchDelegate& operator=(
      const WaylandPointerGesturePinchDelegate&) = delete;

  ~WaylandPointerGesturePinchDelegate() override {
    if (pointer_)
      pointer_->SetGesturePinchDelegate(nullptr);
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerPinchBegin(uint32_t unique_touch_event_id,
                           base::TimeTicks time_stamp,
                           Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    zwp_pointer_gesture_pinch_v1_send_begin(resource_, unique_touch_event_id,
                                            TimeTicksToMilliseconds(time_stamp),
                                            surface_resource, 2);
  }
  void OnPointerPinchUpdate(base::TimeTicks time_stamp, float scale) override {
    zwp_pointer_gesture_pinch_v1_send_update(
        resource_, TimeTicksToMilliseconds(time_stamp), 0, 0,
        wl_fixed_from_double(scale), 0);
  }
  void OnPointerPinchEnd(uint32_t unique_touch_event_id,
                         base::TimeTicks time_stamp) override {
    zwp_pointer_gesture_pinch_v1_send_end(resource_, unique_touch_event_id,
                                          TimeTicksToMilliseconds(time_stamp),
                                          0);
  }

 private:
  const raw_ptr<wl_resource> resource_;
  raw_ptr<Pointer> pointer_;
};

void pointer_gesture_pinch_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_pointer_gesture_pinch_v1_interface
    pointer_gesture_pinch_implementation = {pointer_gesture_pinch_destroy};

void pointer_gestures_get_pinch_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  wl_resource* pointer_gesture_pinch_resource = wl_resource_create(
      client, &zwp_pointer_gesture_pinch_v1_interface, 1, id);
  SetImplementation(pointer_gesture_pinch_resource,
                    &pointer_gesture_pinch_implementation,
                    std::make_unique<WaylandPointerGesturePinchDelegate>(
                        pointer_gesture_pinch_resource, pointer));
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gestures_v1 interface:

const struct zwp_pointer_gestures_v1_interface pointer_gestures_implementation =
    {pointer_gestures_get_swipe_gesture, pointer_gestures_get_pinch_gesture};

}  // namespace

void bind_pointer_gestures(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_pointer_gestures_v1_interface, version, id);
  wl_resource_set_implementation(resource, &pointer_gestures_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
