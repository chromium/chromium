// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_
#define COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_

#include <surface-augmenter-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

// Clients at version 8 think clip rect is in parent surface's space, while
// clients at version 9 or above think it's in local surface's space.
// Unfortunately, clipping in version 9 is implemented incorrectly. It has been
// fixed in version 10, so use version 10 instead.
constexpr uint32_t kSurfaceAugmenterVersion =
    AUGMENTED_SURFACE_SET_CLIP_RECT_SINCE_VERSION + 2;

void bind_surface_augmenter(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_SURFACE_AUGMENTER_H_
