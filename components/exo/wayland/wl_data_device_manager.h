// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_client;

namespace exo {
class Display;

namespace wayland {
class SerialTracker;

constexpr uint32_t kWlDataDeviceManagerVersion = 3;

struct WaylandDataDeviceManager {
  WaylandDataDeviceManager(Display* display, SerialTracker* serial_tracker)
      : display(display), serial_tracker(serial_tracker) {}

  // Owned by WaylandServerController, which always outlives
  // wl_data_device_manager.
  Display* const display;

  // Owned by Server, which always outlives wl_data_device_manager.
  SerialTracker* const serial_tracker;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceManager);
};

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_
