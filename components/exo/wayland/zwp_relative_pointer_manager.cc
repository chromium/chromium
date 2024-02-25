// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <memory>

#include "base/memory/raw_ptr.h"
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
  }

  WaylandRelativePointerDelegate(const WaylandRelativePointerDelegate&) =
      delete;
  WaylandRelativePointerDelegate& operator=(
      const WaylandRelativePointerDelegate&) = delete;

  ~WaylandRelativePointerDelegate() override {
    if (pointer_)
      pointer_->UnregisterRelativePointerDelegate(this);
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerRelativeMotion(base::TimeTicks time_stamp,
                               const gfx::Vector2dF& relative_motion,
                               const gfx::Vector2dF& ordinal_motion) override {
    zwp_relative_pointer_v1_send_relative_motion(
        resource_,                                  // resource
        0,                                          // utime_hi
        TimeTicksToMilliseconds(time_stamp),        // utime_lo
        wl_fixed_from_double(relative_motion.x()),  // dx
        wl_fixed_from_double(relative_motion.y()),  // dy
        wl_fixed_from_double(ordinal_motion.x()),   // dx_unaccel
        wl_fixed_from_double(ordinal_motion.y()));  // dy_unaccel
  }

 private:
  const raw_ptr<wl_resource> resource_;
  raw_ptr<Pointer> pointer_;
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
