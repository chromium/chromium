// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SCHEDULER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SCHEDULER_H_

#include "base/time/time.h"
#include "components/metrics/metrics_rotation_scheduler.h"

namespace metrics::structured {

// Schedulers a periodic rotation of logs and initiates a log upload to the
// reporting service.
class StructuredMetricsScheduler : public metrics::MetricsRotationScheduler {
 public:
  // Creates StructuredMetricsScheduler object with the given
  // |rotation_callback| callback to call when log rotation should happen and
  // |interval_callback| to determine the interval between rotations in steady
  // state.
  StructuredMetricsScheduler(
      const base::RepeatingClosure& rotation_callback,
      const base::RepeatingCallback<base::TimeDelta(void)>& interval_callback,
      bool fast_startup_for_testing);

  StructuredMetricsScheduler(const StructuredMetricsScheduler&) = delete;
  StructuredMetricsScheduler& operator=(const StructuredMetricsScheduler&) =
      delete;

  ~StructuredMetricsScheduler() override;

 protected:
  void LogMetricsInitSequence(InitSequence sequence) override;
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_SCHEDULER_H_
