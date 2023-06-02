// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_scheduler.h"

#include "base/metrics/histogram_functions.h"

namespace metrics::structured {
StructuredMetricsScheduler::StructuredMetricsScheduler(
    const base::RepeatingClosure& rotation_callback,
    const base::RepeatingCallback<base::TimeDelta(void)>& interval_callback,
    bool fast_startup_for_testing)
    : metrics::MetricsRotationScheduler(rotation_callback,
                                        interval_callback,
                                        fast_startup_for_testing) {}

StructuredMetricsScheduler::~StructuredMetricsScheduler() = default;

void StructuredMetricsScheduler::LogMetricsInitSequence(InitSequence sequence) {
  base::UmaHistogramEnumeration("StructuredMetrics.InitSequence", sequence,
                                INIT_SEQUENCE_ENUM_SIZE);
}

}  // namespace metrics::structured
