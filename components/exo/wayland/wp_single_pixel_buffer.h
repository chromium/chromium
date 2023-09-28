// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WP_SINGLE_PIXEL_BUFFER_H_
#define COMPONENTS_EXO_WAYLAND_WP_SINGLE_PIXEL_BUFFER_H_

#include <single-pixel-buffer-v1-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

constexpr uint32_t kSinglePixelBufferVersion =
    WP_SINGLE_PIXEL_BUFFER_MANAGER_V1_DESTROY_SINCE_VERSION;

void bind_single_pixel_buffer(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_WP_SINGLE_PIXEL_BUFFER_H_
