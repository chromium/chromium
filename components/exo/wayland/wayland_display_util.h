// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_UTIL_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_UTIL_H_

#include <wayland-server-protocol-core.h>

#include "ui/display/display.h"

namespace exo {
namespace wayland {

// Returns the transform that a compositor will apply to a surface to
// compensate for the rotation of an output device.
wl_output_transform OutputTransform(display::Display::Rotation rotation);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_UTIL_H_
