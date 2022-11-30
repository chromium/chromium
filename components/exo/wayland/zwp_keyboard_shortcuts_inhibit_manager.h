// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_H_

#include <stdint.h>

struct wl_client;

namespace exo::wayland {

void bind_keyboard_shortcuts_inhibit_manager(wl_client* client,
                                             void* data,
                                             uint32_t version,
                                             uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_H_
