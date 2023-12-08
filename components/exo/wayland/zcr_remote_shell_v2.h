// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_

#include <remote-shell-unstable-v2-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

// version: 6
constexpr uint32_t kZcrRemoteShellV2Version =
    ZCR_REMOTE_SURFACE_V2_SET_SHADOW_CORNER_RADII_SINCE_VERSION;

void bind_remote_shell_v2(wl_client* client,
                          void* data,
                          uint32_t version,
                          uint32_t id);
}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_
