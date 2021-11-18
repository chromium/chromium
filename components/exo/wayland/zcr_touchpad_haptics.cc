// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_touchpad_haptics.h"

#include <touchpad-haptics-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

namespace exo {
namespace wayland {
namespace {

void touchpad_haptics_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void touchpad_haptics_play(wl_client* client,
                           wl_resource* resource,
                           uint32_t effect,
                           int32_t strength) {
  // TODO(b/205702807): Call InputController::PlayHapticTouchpadEffect.
}

const struct zcr_touchpad_haptics_v1_interface touchpad_haptics_implementation =
    {
        touchpad_haptics_destroy,
        touchpad_haptics_play,
};

}  // namespace

void bind_touchpad_haptics(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_touchpad_haptics_v1_interface, version, id);

  wl_resource_set_implementation(resource, &touchpad_haptics_implementation,
                                 nullptr, nullptr);
}

}  // namespace wayland
}  // namespace exo
