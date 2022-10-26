// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_METRICS_LOGGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_METRICS_LOGGER_H_

#include <string>

#include "base/time/time.h"

namespace ash::secure_channel {

// Enumeration of possible connection result when connecting via Nearby
// Connection library. Keep in sync with corresponding enum in
// tools/metrics/histograms/enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class NearbyInitiatorConnectionResult {
  kConnectionSuccess = 0,
  // Numbers 1-3 are deprecated and should not be reused.
  kConnectivityError = 4,
  kAuthenticationError = 5,
  kMaxValue = kAuthenticationError,
};

// Logs a given connection result.
void LogNearbyInitiatorConnectionResult(
    NearbyInitiatorConnectionResult connection_result);

// Logs metrics related to connection latencies. This function should be
// utilized instead of the default UMA_HISTOGRAM_TIMES() macro because it
// provides custom bucket sizes (e.g., UMA_HISTOGRAM_TIMES() only allows
// durations up to 10 seconds, but some connection attempts take longer than
// that).
void LogLatencyMetric(const std::string& metric_name,
                      const base::TimeDelta& duration);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_METRICS_LOGGER_H_
