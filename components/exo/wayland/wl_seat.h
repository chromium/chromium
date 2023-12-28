// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WL_SEAT_H_
#define COMPONENTS_EXO_WAYLAND_WL_SEAT_H_

#include <wayland-server-protocol-core.h>

#include <stdint.h>

#include "base/memory/raw_ptr.h"

namespace exo {
class Seat;

namespace wayland {
class SerialTracker;

constexpr uint32_t kWlSeatVersion = WL_TOUCH_SHAPE_SINCE_VERSION;

struct WaylandSeat {
  WaylandSeat(Seat* seat, SerialTracker* serial_tracker)
      : seat(seat), serial_tracker(serial_tracker) {}
  WaylandSeat(const WaylandSeat&) = delete;
  WaylandSeat& operator=(const WaylandSeat&) = delete;

  // Owned by Display, which always outlives wl_seat.
  const raw_ptr<Seat> seat;

  // Owned by Server, which always outlives wl_seat.
  const raw_ptr<SerialTracker> serial_tracker;
};

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WL_SEAT_H_
