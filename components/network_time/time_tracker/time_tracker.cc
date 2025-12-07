// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/time_tracker/time_tracker.h"

#include "base/logging.h"

namespace {
// Amount of divergence allowed between wall clock and tick clock.
const uint32_t kClockDivergenceSeconds = 60;
}  // namespace

namespace network_time {
TimeTracker::TimeTracker(const base::Time& system_time,
                         const base::TimeTicks& system_ticks,
                         const base::Time& time,
                         const base::TimeDelta& uncertainty)
    : state_{system_time, system_ticks, time, uncertainty} {}

bool TimeTracker::GetTime(const base::Time& system_time,
                          const base::TimeTicks& system_ticks,
                          base::Time* time,
                          base::TimeDelta* uncertainty) const {
  base::TimeDelta tick_delta = system_ticks - state_.system_ticks;
  base::TimeDelta time_delta = system_time - state_.system_time;
  if (time_delta.InMilliseconds() < 0) {
    DVLOG(1) << "Time unavailable due to wall clock running backward";
    return false;
  }

  base::TimeDelta divergence = tick_delta - time_delta;
  if (divergence.magnitude() > base::Seconds(kClockDivergenceSeconds)) {
    DVLOG(1) << "Time unavailable due to clocks diverging";
    return false;
  }
  *time = state_.known_time + tick_delta;
  if (uncertainty) {
    *uncertainty = state_.uncertainty + divergence;
  }
  return true;
}

}  // namespace network_time
