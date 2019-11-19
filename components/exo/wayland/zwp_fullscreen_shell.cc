// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_fullscreen_shell.h"

#include <fullscreen-shell-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/bind.h"
#include "components/exo/fullscreen_shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

namespace {

void fullscreen_shell_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void fullscreen_shell_present_surface(wl_client* client,
                                      wl_resource* resource,
                                      wl_resource* surface,
                                      uint32_t method,
                                      wl_resource* output) {
  FullscreenShellSurface* fullscreen_shell_surface =
      GetUserDataAs<FullscreenShellSurface>(resource);
  fullscreen_shell_surface->SetSurface(surface ? GetUserDataAs<Surface>(surface)
                                               : nullptr);
}

void fullscreen_shell_present_surface_for_mode(wl_client* client,
                                               wl_resource* resource,
                                               wl_resource* surface,
                                               wl_resource* output,
                                               int32_t framerate,
                                               uint32_t feedback) {
  NOTIMPLEMENTED();
}

const struct zwp_fullscreen_shell_v1_interface fullscreen_shell_implementation =
    {fullscreen_shell_release, fullscreen_shell_present_surface,
     fullscreen_shell_present_surface_for_mode};

}  // namespace

void bind_fullscreen_shell(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  std::unique_ptr<FullscreenShellSurface> fullscreen_shell_surface =
      std::make_unique<FullscreenShellSurface>();
  fullscreen_shell_surface->SetEnabled(true);
  wl_resource* resource =
      wl_resource_create(client, &zwp_fullscreen_shell_v1_interface, 1, id);
  fullscreen_shell_surface->set_surface_destroyed_callback(
      base::BindOnce(&wl_resource_destroy, base::Unretained(resource)));
  SetImplementation(resource, &fullscreen_shell_implementation,
                    std::move(fullscreen_shell_surface));
}

}  // namespace wayland
}  // namespace exo
