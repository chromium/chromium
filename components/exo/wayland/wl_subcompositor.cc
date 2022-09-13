// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_subcompositor.h"

#include <wayland-server-protocol-core.h>

#include "components/exo/display.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// wl_subsurface_interface:

void subsurface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subsurface_set_position(wl_client* client,
                             wl_resource* resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<SubSurface>(resource)->SetPosition(gfx::PointF(x, y));
}

void subsurface_place_above(wl_client* client,
                            wl_resource* resource,
                            wl_resource* reference_resource) {
  GetUserDataAs<SubSurface>(resource)->PlaceAbove(
      GetUserDataAs<Surface>(reference_resource));
}

void subsurface_place_below(wl_client* client,
                            wl_resource* resource,
                            wl_resource* sibling_resource) {
  GetUserDataAs<SubSurface>(resource)->PlaceBelow(
      GetUserDataAs<Surface>(sibling_resource));
}

void subsurface_set_sync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(true);
}

void subsurface_set_desync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(false);
}

const struct wl_subsurface_interface subsurface_implementation = {
    subsurface_destroy,     subsurface_set_position, subsurface_place_above,
    subsurface_place_below, subsurface_set_sync,     subsurface_set_desync};

////////////////////////////////////////////////////////////////////////////////
// wl_subcompositor_interface:

void subcompositor_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subcompositor_get_subsurface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface,
                                  wl_resource* parent) {
  std::unique_ptr<SubSurface> subsurface =
      GetUserDataAs<Display>(resource)->CreateSubSurface(
          GetUserDataAs<Surface>(surface), GetUserDataAs<Surface>(parent));
  if (!subsurface) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "invalid surface");
    return;
  }

  wl_resource* subsurface_resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);

  SetImplementation(subsurface_resource, &subsurface_implementation,
                    std::move(subsurface));
}

const struct wl_subcompositor_interface subcompositor_implementation = {
    subcompositor_destroy, subcompositor_get_subsurface};

}  // namespace

void bind_subcompositor(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);

  wl_resource_set_implementation(resource, &subcompositor_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
