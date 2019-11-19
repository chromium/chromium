// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_client;

namespace exo {
class Display;

namespace wayland {
class SerialTracker;

struct WaylandXdgShell {
  WaylandXdgShell(Display* display, SerialTracker* serial_tracker)
      : display(display), serial_tracker(serial_tracker) {}

  // Owned by WaylandServerController, which always outlives zxdg_shell.
  Display* const display;

  // Owned by Server, which always outlives zxdg_shell.
  SerialTracker* const serial_tracker;

  DISALLOW_COPY_AND_ASSIGN(WaylandXdgShell);
};

void bind_xdg_shell_v6(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_
