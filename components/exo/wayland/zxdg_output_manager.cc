// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zxdg_output_manager.h"

#include <xdg-output-unstable-v1-server-protocol.h>

#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"

namespace exo {
namespace wayland {

////////////////////////////////////////////////////////////////////////////////
// zxdg_output_v1_interface

// wl_resource_destroy_func_t for xdg_output wl_resource
void xdg_output_destroy(wl_resource* resource) {
  WaylandDisplayHandler* handler =
      GetUserDataAs<WaylandDisplayHandler>(resource);
  if (handler)
    handler->UnsetXdgOutputResource();
}

void xdg_output_client_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zxdg_output_v1_interface xdg_output_implementation = {
    xdg_output_client_destroy};

////////////////////////////////////////////////////////////////////////////////
// zxdg_output_manager_v1_interface

void xdg_output_manager_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_output_manager_get_xdg_output(wl_client* client,
                                       wl_resource* manager,
                                       uint32_t id,
                                       wl_resource* output_resource) {
  uint32_t version = wl_resource_get_version(manager);
  wl_resource* resource =
      wl_resource_create(client, &zxdg_output_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  WaylandDisplayHandler* handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);
  if (handler) {
    wl_resource_set_implementation(resource, &xdg_output_implementation,
                                   handler, &xdg_output_destroy);
    handler->OnXdgOutputCreated(resource);
  }
}

const struct zxdg_output_manager_v1_interface
    xdg_output_manager_implementation = {xdg_output_manager_destroy,
                                         xdg_output_manager_get_xdg_output};

void bind_zxdg_output_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zxdg_output_manager_v1_interface, version, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &xdg_output_manager_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
