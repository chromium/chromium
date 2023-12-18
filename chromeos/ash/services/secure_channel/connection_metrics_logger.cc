// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace ash::secure_channel {

namespace {

constexpr const base::TimeDelta kMinLatencyDuration = base::Milliseconds(1);
constexpr const base::TimeDelta kMaxLatencyDuration = base::Seconds(45);

// Provide enough granularity so that durations <10s are assigned to buckets
// in the hundreds of milliseconds.
const int kNumMetricsBuckets = 100;

}  // namespace

void LogNearbyInitiatorConnectionResult(
    NearbyInitiatorConnectionResult connection_result) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.ConnectionResult", connection_result);
}

void LogLatencyMetric(const std::string& metric_name,
                      const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(metric_name, duration, kMinLatencyDuration,
                                kMaxLatencyDuration, kNumMetricsBuckets);
}

}  // namespace ash::secure_channel
