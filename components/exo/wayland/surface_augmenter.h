// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_
#define COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_

#include <surface-augmenter-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

// version: 12
constexpr uint32_t kSurfaceAugmenterVersion =
    AUGMENTED_SURFACE_SET_FRAME_TRACE_ID_SINCE_VERSION + 1;

void bind_surface_augmenter(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_
