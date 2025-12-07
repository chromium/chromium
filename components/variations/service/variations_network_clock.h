// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_NETWORK_CLOCK_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_NETWORK_CLOCK_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "components/network_time/time_tracker/time_tracker.h"

namespace variations {

// A thread-safe clock implementation that tracks network time.
class VariationsNetworkClock
    : public base::Clock,
      public network_time::NetworkTimeTracker::NetworkTimeObserver {
 public:
  // `network_time_tracker` must be non-null. The VariationsNetworkClock and
  // NetworkTimeTracker correctly handle either being destroyed first.
  explicit VariationsNetworkClock(
      network_time::NetworkTimeTracker* network_time_tracker);

  ~VariationsNetworkClock() override;

  VariationsNetworkClock(const VariationsNetworkClock&) = delete;
  VariationsNetworkClock& operator=(const VariationsNetworkClock&) = delete;

  // base::Clock:
  base::Time Now() const override;

  // network_time::NetworkTimeTracker::NetworkTimeObserver:
  void OnNetworkTimeChanged(
      network_time::TimeTracker::TimeTrackerState state) override;

 private:
  // Updates the `time_tracker_` with the given state.
  void UpdateTimeTracker(
      const network_time::TimeTracker::TimeTrackerState& state);

  mutable base::Lock lock_;

  // Tracks "known" time vs timeticks to estimate network time.
  std::optional<network_time::TimeTracker> time_tracker_ GUARDED_BY(lock_);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_NETWORK_CLOCK_H_
