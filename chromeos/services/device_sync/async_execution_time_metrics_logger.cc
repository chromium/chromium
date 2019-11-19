// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/async_execution_time_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {

namespace device_sync {

namespace {

constexpr const base::TimeDelta kMinAsyncExecutionTime =
    base::TimeDelta::FromMilliseconds(1);

// Provide enough granularity so that durations <10s are assigned to buckets in
// the hundreds of milliseconds.
const int kNumBuckets = 100;

}  // namespace

void LogAsyncExecutionTimeMetric(const std::string& metric_name,
                                 const base::TimeDelta& execution_time) {
  base::UmaHistogramCustomTimes(metric_name, execution_time,
                                kMinAsyncExecutionTime, kMaxAsyncExecutionTime,
                                kNumBuckets);
}

}  // namespace device_sync

}  // namespace chromeos
