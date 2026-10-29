// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/colocation/colocation_time_source.h"

#include "base/check.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"

namespace browser_actuator {

base::Time GetColocationProofTime(network_time::NetworkTimeTracker* tracker,
                                  const base::Clock* clock) {
  CHECK(clock);
  base::Time network_time;
  if (tracker && tracker->GetNetworkTime(&network_time,
                                         /*uncertainty=*/nullptr) ==
                     network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    return network_time;
  }
  return clock->Now();
}

}  // namespace browser_actuator
