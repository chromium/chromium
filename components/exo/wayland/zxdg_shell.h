// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"

struct wl_client;

namespace exo {
class Display;

namespace wayland {
class SerialTracker;

struct WaylandZxdgShell {
  WaylandZxdgShell(Display* display, SerialTracker* serial_tracker)
      : display(display), serial_tracker(serial_tracker) {}

  WaylandZxdgShell(const WaylandZxdgShell&) = delete;
  WaylandZxdgShell& operator=(const WaylandZxdgShell&) = delete;

  // Owned by WaylandServerController, which always outlives zxdg_shell.
  const raw_ptr<Display, ExperimentalAsh> display;

  // Owned by Server, which always outlives zxdg_shell.
  const raw_ptr<SerialTracker, ExperimentalAsh> serial_tracker;
};

void bind_zxdg_shell_v6(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZXDG_SHELL_H_
