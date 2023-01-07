// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/hotspot_usage_duration_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace tether {

namespace {

// Minimum value for the usage duration metric.
const int64_t kMinDurationSeconds = 1;

// Maximum value for the usage duration metric.
const int64_t kMaxDurationHours = 5;

// Number of buckets in the metric.
const int kNumMetricsBuckets = 100;

}  // namespace

HotspotUsageDurationTracker::HotspotUsageDurationTracker(
    ActiveHost* active_host,
    base::Clock* clock)
    : active_host_(active_host), clock_(clock) {
  active_host_->AddObserver(this);
}

HotspotUsageDurationTracker::~HotspotUsageDurationTracker() {
  active_host_->RemoveObserver(this);
}

void HotspotUsageDurationTracker::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  // Handle the case that a session is being tracked but the active host's
  // status changes unexpectedly.
  HandleUnexpectedCurrentSession(change_info.new_status);

  switch (change_info.new_status) {
    case ActiveHost::ActiveHostStatus::CONNECTED:
      last_connection_start_ = clock_->Now();
      break;
    case ActiveHost::ActiveHostStatus::DISCONNECTED: {
      // If |last_connection_start_| has not been set, there was no active
      // connection before this status change; thus, there is nothing to do.
      if (last_connection_start_.is_null())
        break;

      base::TimeDelta duration = clock_->Now() - last_connection_start_;

      // Reset |last_connection_start_|; it will be set again the next time that
      // a connection is established.
      last_connection_start_ = base::Time();

      PA_LOG(VERBOSE) << "Connection to hotspot has ended. Duration was "
                      << duration.InSeconds() << " second(s).";
      UMA_HISTOGRAM_CUSTOM_TIMES("InstantTethering.HotspotUsageDuration",
                                 duration,
                                 base::Seconds(kMinDurationSeconds) /* min */,
                                 base::Hours(kMaxDurationHours) /* max */,
                                 kNumMetricsBuckets /* bucket_count */);
      break;
    }
    default:
      break;
  }
}

void HotspotUsageDurationTracker::HandleUnexpectedCurrentSession(
    const ActiveHost::ActiveHostStatus& active_host_status) {
  // If there is no start timestamp, no session is being tracked.
  if (last_connection_start_.is_null())
    return;

  // It is expected that when a current session ends, the active host's status
  // will change to DISCONNECTED.
  if (active_host_status == ActiveHost::ActiveHostStatus::DISCONNECTED)
    return;

  base::TimeDelta previous_duration = clock_->Now() - last_connection_start_;
  PA_LOG(ERROR) << "Active host status changed to "
                << ActiveHost::StatusToString(active_host_status) << ", but a "
                << "session was already being tracked ("
                << previous_duration.InSeconds() << " second(s)). "
                << "Not recording any metrics for this session since this "
                << "situation was reached in error.";

  // Delete the erroneously started session.
  last_connection_start_ = base::Time();
}

}  // namespace tether

}  // namespace ash
