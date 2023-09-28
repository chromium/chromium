// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_STYLUS_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_STYLUS_H_

#include <stylus-unstable-v2-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

constexpr uint32_t kZcrStylusVersion =
    ZCR_STYLUS_V2_GET_POINTER_STYLUS_SINCE_VERSION;

void bind_stylus_v2(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_STYLUS_H_
