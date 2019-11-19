// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_output.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wm_helper.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

namespace exo {
namespace wayland {

////////////////////////////////////////////////////////////////////////////////
// wl_output_interface:

void output_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_output_interface output_implementation = {output_release};

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  WaylandDisplayOutput* output = static_cast<WaylandDisplayOutput*>(data);

  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, kWlOutputVersion), id);

  SetImplementation(
      resource, &output_implementation,
      std::make_unique<WaylandDisplayObserver>(output->id(), resource));
}

}  // namespace wayland
}  // namespace exo
