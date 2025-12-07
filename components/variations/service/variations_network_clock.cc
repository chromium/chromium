// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_network_clock.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "components/network_time/time_tracker/time_tracker.h"

namespace variations {

namespace {

// The source of time used by the VariationsNetworkClock. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused. Keep these values in sync with the VariationsTimeSource
// enum in //tools/metrics/histograms/metadata/variations/enums.xml.
enum class TimeSource {
  kUnknown = 0,
  kLocal = 1,
  kNetwork = 2,
  kMaxValue = kNetwork,
};

// Generates a histogram sample indicating whether the VariationsNetworkClock is
// using network time or falling back to local time.
void LogTimeSource(TimeSource time_source) {
  base::UmaHistogramEnumeration("Variations.Headers.TimeSource", time_source);
}

}  // namespace

VariationsNetworkClock::VariationsNetworkClock(
    network_time::NetworkTimeTracker* tracker)
    : network_time::NetworkTimeTracker::NetworkTimeObserver(tracker) {
  network_time::TimeTracker::TimeTrackerState state;
  if (tracker->GetTrackerState(&state)) {
    UpdateTimeTracker(state);
  }
}

VariationsNetworkClock::~VariationsNetworkClock() = default;

base::Time VariationsNetworkClock::Now() const {
  const base::Time local_time = base::Time::Now();
  const base::TimeTicks local_ticks = base::TimeTicks::Now();
  base::Time estimated_time;

  base::AutoLock lock(lock_);
  if (time_tracker_.has_value() &&
      time_tracker_->GetTime(local_time, local_ticks, &estimated_time,
                             /*uncertainty=*/nullptr)) {
    LogTimeSource(TimeSource::kNetwork);
    return estimated_time;
  }
  LogTimeSource(TimeSource::kLocal);
  return local_time;
}

void VariationsNetworkClock::OnNetworkTimeChanged(
    network_time::TimeTracker::TimeTrackerState state) {
  UpdateTimeTracker(state);
}

void VariationsNetworkClock::UpdateTimeTracker(
    const network_time::TimeTracker::TimeTrackerState& state) {
  base::AutoLock lock(lock_);
  time_tracker_.emplace(state.system_time, state.system_ticks, state.known_time,
                        state.uncertainty);
}

}  // namespace variations
