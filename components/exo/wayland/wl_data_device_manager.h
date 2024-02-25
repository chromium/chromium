// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_

#include <wayland-server-protocol-core.h>

#include <stdint.h>

#include "base/memory/raw_ptr.h"

namespace exo {
class Display;

namespace wayland {
class SerialTracker;

constexpr uint32_t kWlDataDeviceManagerVersion =
    WL_DATA_OFFER_FINISH_SINCE_VERSION;

struct WaylandDataDeviceManager {
  WaylandDataDeviceManager(Display* display, SerialTracker* serial_tracker)
      : display(display), serial_tracker(serial_tracker) {}

  WaylandDataDeviceManager(const WaylandDataDeviceManager&) = delete;
  WaylandDataDeviceManager& operator=(const WaylandDataDeviceManager&) = delete;

  // Owned by WaylandServerController, which always outlives
  // wl_data_device_manager.
  const raw_ptr<Display> display;

  // Owned by Server, which always outlives wl_data_device_manager.
  const raw_ptr<SerialTracker> serial_tracker;
};

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WL_DATA_DEVICE_MANAGER_H_
