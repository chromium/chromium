// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/timer/arc_timer_mojom_traits.h"

#include <utility>

#include "base/notreached.h"

namespace mojo {

// static
arc::mojom::ClockId EnumTraits<arc::mojom::ClockId, clockid_t>::ToMojom(
    clockid_t clock_id) {
  switch (clock_id) {
    case CLOCK_REALTIME_ALARM:
      return arc::mojom::ClockId::REALTIME_ALARM;
    case CLOCK_BOOTTIME_ALARM:
      return arc::mojom::ClockId::BOOTTIME_ALARM;
  }
  NOTREACHED();
}

// static
std::optional<clockid_t> EnumTraits<arc::mojom::ClockId, clockid_t>::FromMojom(
    arc::mojom::ClockId input) {
  switch (input) {
    case arc::mojom::ClockId::REALTIME_ALARM:
      return CLOCK_REALTIME_ALARM;
    case arc::mojom::ClockId::BOOTTIME_ALARM:
      return CLOCK_BOOTTIME_ALARM;
  }
  NOTREACHED();
}

}  // namespace mojo
