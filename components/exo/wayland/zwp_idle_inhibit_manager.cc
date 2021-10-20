// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_idle_inhibit_manager.h"

#include <idle-inhibit-unstable-v1-server-protocol.h>
#include "wayland-server-core.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// zwp_idle_inhibitor_v1_interface:

void zwp_idle_inhibitor_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_idle_inhibitor_v1_interface
    zwp_idle_inhibitor_v1_implementation = {zwp_idle_inhibitor_destroy};

////////////////////////////////////////////////////////////////////////////////
// zwp_idle_inhibit_manager_v1_interface:

void zwp_idle_inhibit_manager_destroy(wl_client* client,
                                      wl_resource* resource) {
  wl_resource_destroy(resource);
}

void zwp_idle_inhibit_manager_create_inhibitor(wl_client* client,
                                               wl_resource* resource,
                                               uint32_t id,
                                               wl_resource* surface) {
  wl_resource* inhibitor =
      wl_resource_create(client, &zwp_idle_inhibitor_v1_interface,
                         wl_resource_get_version(resource), id);
  wl_resource_set_implementation(inhibitor,
                                 &zwp_idle_inhibitor_v1_implementation,
                                 /*data=*/nullptr, /*destroy=*/nullptr);
}

const struct zwp_idle_inhibit_manager_v1_interface
    zwp_idle_inhibit_manager_v1_implementation = {
        zwp_idle_inhibit_manager_destroy,
        zwp_idle_inhibit_manager_create_inhibitor};

}  // namespace

void bind_zwp_idle_inhibit_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_idle_inhibit_manager_v1_interface, version, id);
  wl_resource_set_implementation(resource,
                                 &zwp_idle_inhibit_manager_v1_implementation,
                                 data, /*destroy=*/nullptr);
}

}  // namespace wayland
}  // namespace exo
