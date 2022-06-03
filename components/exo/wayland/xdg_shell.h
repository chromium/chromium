// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_

#include <stdint.h>

struct wl_client;
struct wl_resource;

namespace exo {
class Display;
class ShellSurfaceBase;

namespace wayland {
class SerialTracker;

struct WaylandXdgShell {
  WaylandXdgShell(Display* display, SerialTracker* serial_tracker)
      : display(display), serial_tracker(serial_tracker) {}

  WaylandXdgShell(const WaylandXdgShell&) = delete;
  WaylandXdgShell& operator=(const WaylandXdgShell&) = delete;

  // Owned by WaylandServerController, which always outlives xdg_shell.
  Display* const display;

  // Owned by Server, which always outlives xdg_shell.
  SerialTracker* const serial_tracker;
};

void bind_xdg_shell(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id);

ShellSurfaceBase* GetShellSurfaceFromToplevelResource(
    wl_resource* surface_resource);

ShellSurfaceBase* GetShellSurfaceFromPopupResource(
    wl_resource* surface_resource);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_
