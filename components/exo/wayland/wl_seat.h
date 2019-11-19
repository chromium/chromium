// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WL_SEAT_H_
#define COMPONENTS_EXO_WAYLAND_WL_SEAT_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_client;

namespace exo {
class Seat;

namespace wayland {
class SerialTracker;

constexpr uint32_t kWlSeatVersion = 6;

struct WaylandSeat {
  WaylandSeat(Seat* seat, SerialTracker* serial_tracker)
      : seat(seat), serial_tracker(serial_tracker) {}

  // Owned by Display, which always outlives wl_seat.
  Seat* const seat;

  // Owned by Server, which always outlives wl_seat.
  SerialTracker* const serial_tracker;

  DISALLOW_COPY_AND_ASSIGN(WaylandSeat);
};

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WL_SEAT_H_
