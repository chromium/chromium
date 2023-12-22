// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_

#include <stdint.h>
#include <memory>

#include "base/memory/raw_ptr.h"

struct wl_client;
struct wl_resource;

namespace exo {
class Display;
class ShellSurfaceBase;
class ShellSurface;
class XdgShellSurface;

namespace wayland {
class SerialTracker;

struct WaylandXdgShell {
  WaylandXdgShell(Display* display,
                  SerialTracker* serial_tracker,
                  SerialTracker* rotation_serial_tracker)
      : display(display),
        serial_tracker(serial_tracker),
        rotation_serial_tracker(rotation_serial_tracker) {}

  WaylandXdgShell(const WaylandXdgShell&) = delete;
  WaylandXdgShell& operator=(const WaylandXdgShell&) = delete;

  // Owned by WaylandServerController, which always outlives xdg_shell.
  const raw_ptr<Display> display;

  // Owned by Server, which always outlives xdg_shell.
  const raw_ptr<SerialTracker> serial_tracker;
  const raw_ptr<SerialTracker> rotation_serial_tracker;
};

struct WaylandXdgSurface {
  WaylandXdgSurface(std::unique_ptr<XdgShellSurface> shell_surface,
                    SerialTracker* const serial_tracker,
                    SerialTracker* const rotation_serial_tracker);

  ~WaylandXdgSurface();

  WaylandXdgSurface(const WaylandXdgSurface&) = delete;
  WaylandXdgSurface& operator=(const WaylandXdgSurface&) = delete;

  std::unique_ptr<XdgShellSurface> shell_surface;

  // Owned by Server, which always outlives this surface.
  const raw_ptr<SerialTracker> serial_tracker;
  const raw_ptr<SerialTracker> rotation_serial_tracker;
};

void bind_xdg_shell(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id);

struct ShellSurfaceData {
  ShellSurfaceData(ShellSurface* shell_surface,
                   SerialTracker* serial_tracker,
                   SerialTracker* rotation_serial_tracker,
                   wl_resource* surface_resource)
      : shell_surface(shell_surface),
        serial_tracker(serial_tracker),
        rotation_serial_tracker(rotation_serial_tracker),
        surface_resource(surface_resource) {}

  ShellSurfaceData(const ShellSurfaceData&) = delete;
  ShellSurfaceData& operator=(const ShellSurfaceData&) = delete;

  const raw_ptr<ShellSurface> shell_surface;

  // Owned by Server, which always outlives xdg_shell.
  const raw_ptr<SerialTracker> serial_tracker;
  const raw_ptr<SerialTracker> rotation_serial_tracker;

  const raw_ptr<wl_resource> surface_resource;
};

ShellSurfaceData GetShellSurfaceFromToplevelResource(
    wl_resource* surface_resource);

ShellSurfaceBase* GetShellSurfaceFromPopupResource(
    wl_resource* surface_resource);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_XDG_SHELL_H_
