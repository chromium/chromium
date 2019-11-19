// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZWP_TEXT_INPUT_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_ZWP_TEXT_INPUT_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_client;

namespace exo {
namespace wayland {
class SerialTracker;

struct WaylandTextInputManager {
  WaylandTextInputManager(SerialTracker* serial_tracker)
      : serial_tracker(serial_tracker) {}

  // Owned by Server, which always outlives zwp_text_input_manager.
  SerialTracker* const serial_tracker;

  DISALLOW_COPY_AND_ASSIGN(WaylandTextInputManager);
};

void bind_text_input_manager(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZWP_TEXT_INPUT_MANAGER_H_
