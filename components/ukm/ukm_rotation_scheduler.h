// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_
#define COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_

#include "base/time/time.h"
#include "components/metrics/metrics_rotation_scheduler.h"

namespace ukm {

// Scheduler to drive a UkmService object's log rotations.
class UkmRotationScheduler : public metrics::MetricsRotationScheduler {
 public:
  // Creates UkmRotationScheduler object with the given |rotation_callback|
  // callback to call when log rotation should happen and |interval_callback|
  // to determine the interval between rotations in steady state.
  UkmRotationScheduler(
      const base::RepeatingClosure& rotation_callback,
      bool fast_startup_for_testing,
      const base::RepeatingCallback<base::TimeDelta(void)>& interval_callback);
  ~UkmRotationScheduler() override;

 private:
  // Record the init sequence order histogram.
  void LogMetricsInitSequence(InitSequence sequence) override;

  DISALLOW_COPY_AND_ASSIGN(UkmRotationScheduler);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_
