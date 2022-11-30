// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_

#include <cinttypes>

struct wl_client;

namespace exo {
namespace wayland {

void bind_touchpad_haptics(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_
