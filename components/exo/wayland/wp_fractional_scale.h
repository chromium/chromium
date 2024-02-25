// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WP_FRACTIONAL_SCALE_H_
#define COMPONENTS_EXO_WAYLAND_WP_FRACTIONAL_SCALE_H_

#include <fractional-scale-v1-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

constexpr uint32_t kFractionalScaleVersion =
    WP_FRACTIONAL_SCALE_MANAGER_V1_GET_FRACTIONAL_SCALE_SINCE_VERSION;

void bind_fractional_scale_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_WP_FRACTIONAL_SCALE_H_
