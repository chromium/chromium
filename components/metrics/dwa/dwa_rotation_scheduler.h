// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_ROTATION_SCHEDULER_H_
#define COMPONENTS_METRICS_DWA_DWA_ROTATION_SCHEDULER_H_

#include "base/time/time.h"
#include "components/metrics/metrics_rotation_scheduler.h"

namespace metrics::dwa {

// Scheduler to drive a DwaService object's log rotations.
class DwaRotationScheduler : public metrics::MetricsRotationScheduler {
 public:
  // Creates DwaRotationScheduler object with the given `rotation_callback`
  // callback to call when log rotation should happen and `interval_callback`
  // to determine the interval between rotations in steady state.
  DwaRotationScheduler(
      const base::RepeatingClosure& rotation_callback,
      const base::RepeatingCallback<base::TimeDelta(void)>& interval_callback,
      bool fast_startup);

  DwaRotationScheduler(const DwaRotationScheduler&) = delete;
  DwaRotationScheduler& operator=(const DwaRotationScheduler&) = delete;

  ~DwaRotationScheduler() override;

 private:
  // Record the init sequence order histogram.
  void LogMetricsInitSequence(InitSequence sequence) override;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_ROTATION_SCHEDULER_H_
