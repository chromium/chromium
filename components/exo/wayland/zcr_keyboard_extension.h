// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_EXTENSION_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_EXTENSION_H_

#include <stdint.h>

struct wl_client;

namespace exo {
namespace wayland {

void bind_keyboard_extension(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_EXTENSION_H_
