// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_
#define COMPONENTS_EXO_WAYLAND_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_

#include <stdint.h>

struct wl_client;

namespace exo {
class Surface;

namespace wayland {

void bind_linux_explicit_synchronization(wl_client* client,
                                         void* data,
                                         uint32_t version,
                                         uint32_t id);

// Validates that |surface| adheres to the commit-time restrictions of the
// zwp_linux_surface_synchronization interface. If any rules are violated the
// function emits an error to the client and returns false. Otherwise, the
// function returns true.
bool linux_surface_synchronization_validate_commit(Surface* surface);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_H_
