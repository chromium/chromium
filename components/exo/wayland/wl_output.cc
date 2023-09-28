// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_output.h"

#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wm_helper.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

namespace exo::wayland {

////////////////////////////////////////////////////////////////////////////////
// wl_output_interface:

void output_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_output_interface output_implementation = {output_release};

void bind_output(wl_client* client,
                 void* data,
                 uint32_t version,
                 uint32_t output_id) {
  WaylandDisplayOutput* output = static_cast<WaylandDisplayOutput*>(data);

  wl_resource* resource =
      wl_resource_create(client, &wl_output_interface,
                         std::min(version, kWlOutputVersion), output_id);
  auto handler = std::make_unique<WaylandDisplayHandler>(output, resource);
  SetImplementation(resource, &output_implementation, std::move(handler));

  // Ensure we set the user_data implementation before initializing the output
  // as the user_data's lifetime is expected to correlate to that of the
  // wl_output resource.
  GetUserDataAs<WaylandDisplayHandler>(resource)->Initialize();
}

}  // namespace exo::wayland
