// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WL_COMPOSITOR_H_
#define COMPONENTS_EXO_WAYLAND_WL_COMPOSITOR_H_

#include <wayland-server-protocol-core.h>

#include <stdint.h>

namespace exo::wayland {

constexpr uint32_t kWlCompositorVersion =
    WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION;

void bind_compositor(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_WL_COMPOSITOR_H_
