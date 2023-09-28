// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_CONFIGURATION_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_CONFIGURATION_H_

#include <keyboard-configuration-unstable-v1-server-protocol.h>

#include <stdint.h>

namespace exo::wayland {

constexpr uint32_t kZcrKeyboardConfigurationVersion =
    ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_LAYOUT_INSTALL_SINCE_VERSION;

void bind_keyboard_configuration(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id);

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_KEYBOARD_CONFIGURATION_H_
