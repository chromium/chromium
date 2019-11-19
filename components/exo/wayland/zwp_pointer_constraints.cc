// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_pointer_constraints.h"

#include <pointer-constraints-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <cstdarg>
#include <memory>

#include "components/exo/pointer.h"
#include "components/exo/pointer_constraint_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace exo {
namespace wayland {

namespace {

class WaylandPointerConstraintDelegate : public PointerConstraintDelegate {
 public:
  WaylandPointerConstraintDelegate(wl_resource* constraint_resource,
                                   Surface* surface,
                                   Pointer* pointer,
                                   SkRegion* region,
                                   uint32_t lifetime)
      : constraint_resource_(constraint_resource),
        pointer_(pointer),
        surface_(surface) {
    if (pointer->ConstrainPointer(this))
      EnableConstraint();
    else
      pointer_ = nullptr;
  }

  ~WaylandPointerConstraintDelegate() override {
    if (pointer_)
      pointer_->UnconstrainPointer();
  }

  void OnConstraintBroken() override {
    DisableConstraint();
    pointer_ = nullptr;
  }

  Surface* GetConstrainedSurface() override { return surface_; }

 private:
  void EnableConstraint() {
    zwp_locked_pointer_v1_send_locked(constraint_resource_);
  }

  void DisableConstraint() {
    zwp_locked_pointer_v1_send_unlocked(constraint_resource_);
  }

  wl_resource* constraint_resource_;
  Pointer* pointer_;
  Surface* surface_;
};

////////////////////////////////////////////////////////////////////////////////
// zwp_locked_pointer

void locked_pointer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void locked_pointer_set_cursor_position_hint(wl_client* client,
                                             wl_resource* resource,
                                             wl_fixed_t surface_x,
                                             wl_fixed_t surface_y) {
  // Not supported.
}

void locked_pointer_set_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {
  // Not supported.
}

const struct zwp_locked_pointer_v1_interface locked_pointer_implementation = {
    locked_pointer_destroy, locked_pointer_set_cursor_position_hint,
    locked_pointer_set_region};

////////////////////////////////////////////////////////////////////////////////
// zwp_confined_pointer

void confined_pointer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void confined_pointer_set_region(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* region_resource) {
  // Not supported.
}

const struct zwp_confined_pointer_v1_interface confined_pointer_implementation =
    {
        confined_pointer_destroy,
        confined_pointer_set_region,
};

////////////////////////////////////////////////////////////////////////////////
// zwp_pointer_constraints

void pointer_constraints_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void pointer_constraints_lock_pointer(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t id,
                                      wl_resource* surface_resource,
                                      wl_resource* pointer_resource,
                                      wl_resource* region_resource,
                                      uint32_t lifetime) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  SkRegion* region =
      region_resource ? GetUserDataAs<SkRegion>(region_resource) : nullptr;

  wl_resource* locked_pointer_resource =
      wl_resource_create(client, &zwp_locked_pointer_v1_interface, 1, id);
  SetImplementation(
      locked_pointer_resource, &locked_pointer_implementation,
      std::make_unique<WaylandPointerConstraintDelegate>(
          locked_pointer_resource, surface, pointer, region, lifetime));
}

void pointer_constraints_confine_pointer(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* surface_resource,
                                         wl_resource* pointer_resource,
                                         wl_resource* region_resource,
                                         uint32_t lifetime) {
  // Confined pointer is not currently supported.
  wl_resource* confined_pointer_resource =
      wl_resource_create(client, &zwp_confined_pointer_v1_interface, 1, id);
  SetImplementation<WaylandPointerConstraintDelegate>(
      confined_pointer_resource, &confined_pointer_implementation, nullptr);
}

const struct zwp_pointer_constraints_v1_interface
    pointer_constraints_implementation = {pointer_constraints_destroy,
                                          pointer_constraints_lock_pointer,
                                          pointer_constraints_confine_pointer};

}  // namespace

void bind_pointer_constraints(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_pointer_constraints_v1_interface, version, id);
  wl_resource_set_implementation(resource, &pointer_constraints_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
