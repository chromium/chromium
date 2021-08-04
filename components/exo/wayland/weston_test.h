// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_
#define COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_client;

namespace exo {
class Display;

namespace wayland {

// Tracks button and mouse states for testing.
struct WestonTestState {
  WestonTestState() {}

  bool left_button_pressed = false;
  bool middle_button_pressed = false;
  bool right_button_pressed = false;

  bool control_pressed = false;
  bool alt_pressed = false;
  bool shift_pressed = false;
  bool command_pressed = false;

  DISALLOW_COPY_AND_ASSIGN(WestonTestState);
};

constexpr uint32_t kWestonTestVersion = 1;

void bind_weston_test(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WESTON_TEST_H_
