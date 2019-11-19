// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_H_

#include <stdint.h>

struct wl_client;

namespace gfx {
class Rect;
class Insets;
class Size;
}  // namespace gfx

namespace display {
class Display;
}

namespace exo {
namespace wayland {

constexpr uint32_t kZcrRemoteShellVersion = 24;

void bind_remote_shell(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id);

// Create the insets in client's pixel coordinates in such way that
// work area will be within the chrome's work area.
gfx::Insets GetWorkAreaInsetsInClientPixel(
    const display::Display& display,
    float default_dsf,
    const gfx::Size& size_in_client_pixel,
    const gfx::Rect& work_area_in_dp);

// Returns a work area where the shelf is considered visible.
gfx::Rect GetStableWorkArea(const display::Display& display);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_H_
