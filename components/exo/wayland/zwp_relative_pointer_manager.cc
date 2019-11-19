// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <memory>

#include "components/exo/pointer.h"
#include "components/exo/relative_pointer_delegate.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// relative_pointer_v1 interface:

class WaylandRelativePointerDelegate : public RelativePointerDelegate {
 public:
  WaylandRelativePointerDelegate(wl_resource* resource, Pointer* pointer)
      : resource_(resource), pointer_(pointer) {
    pointer->RegisterRelativePointerDelegate(this);
    // TODO(b/124059008): See below, when requesting relative motion we will
    // also try to gain pointer lock even though the client hasn't asked for it
    // yet...
    pointer_->EnablePointerCapture();
  }

  ~WaylandRelativePointerDelegate() override {
    if (pointer_) {
      // TODO(b/124059008): For whatever reason, exo conflates pointer capture
      // and relative motion. Normally in wayland, removing the relative pointer
      // would not break pointer capture, but in exo that is the case.
      pointer_->DisablePointerCapture();
      pointer_->UnregisterRelativePointerDelegate(this);
    }
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerRelativeMotion(base::TimeTicks time_stamp,
                               const gfx::PointF& relativeMotion) override {
    zwp_relative_pointer_v1_send_relative_motion(
        resource_,                                  // resource
        0,                                          // utime_hi
        TimeTicksToMilliseconds(time_stamp),        // utime_lo
        wl_fixed_from_double(relativeMotion.x()),   // dx
        wl_fixed_from_double(relativeMotion.y()),   // dy
        wl_fixed_from_double(relativeMotion.x()),   // dx_unaccel
        wl_fixed_from_double(relativeMotion.y()));  // dy_unaccel
  }

 private:
  wl_resource* const resource_;
  Pointer* pointer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandRelativePointerDelegate);
};

void relative_pointer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_relative_pointer_v1_interface relative_pointer_impl = {
    relative_pointer_destroy};

////////////////////////////////////////////////////////////////////////////////
// relative_pointer_manager_v1 interface:

void relative_pointer_manager_destroy(wl_client* client,
                                      wl_resource* resource) {
  wl_resource_destroy(resource);
}

void relative_pointer_manager_get_relative_pointer(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* pointer_resource) {
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  wl_resource* relative_pointer_resource =
      wl_resource_create(client, &zwp_relative_pointer_v1_interface, 1, id);
  SetImplementation(relative_pointer_resource, &relative_pointer_impl,
                    std::make_unique<WaylandRelativePointerDelegate>(
                        relative_pointer_resource, pointer));
}

const struct zwp_relative_pointer_manager_v1_interface
    relative_pointer_manager_impl = {
        relative_pointer_manager_destroy,
        relative_pointer_manager_get_relative_pointer};

}  // namespace

void bind_relative_pointer_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_relative_pointer_manager_v1_interface, version, id);
  wl_resource_set_implementation(resource, &relative_pointer_manager_impl, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
