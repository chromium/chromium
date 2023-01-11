// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_ROTATION_SCHEDULER_H_
#define COMPONENTS_METRICS_METRICS_ROTATION_SCHEDULER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

// Scheduler task to drive a MetricsService object's uploading.
class MetricsRotationScheduler : public MetricsScheduler {
 public:
  // Creates MetricsRotationScheduler object with the given |rotation_callback|
  // callback to call when log rotation should happen and |interval_callback|
  // to determine the interval between rotations in steady state.
  // |rotation_callback| must arrange to call RotationFinished on completion.
  MetricsRotationScheduler(
      const base::RepeatingClosure& rotation_callback,
      const base::RepeatingCallback<base::TimeDelta(void)>& interval_callback,
      bool fast_startup_for_testing);

  MetricsRotationScheduler(const MetricsRotationScheduler&) = delete;
  MetricsRotationScheduler& operator=(const MetricsRotationScheduler&) = delete;

  ~MetricsRotationScheduler() override;

  // Callback from MetricsService when the startup init task has completed.
  void InitTaskComplete();

  // Callback from MetricsService when a triggered rotation finishes.
  void RotationFinished();

 protected:
  enum InitSequence {
    TIMER_FIRED_FIRST,
    INIT_TASK_COMPLETED_FIRST,
    INIT_SEQUENCE_ENUM_SIZE,
  };

 private:
  // Record the init sequence order histogram.
  virtual void LogMetricsInitSequence(InitSequence sequence);

  // MetricsScheduler:
  void TriggerTask() override;

  // Whether the InitTaskComplete() callback has been called.
  bool init_task_complete_;

  // Whether the initial scheduled upload timer has fired before the init task
  // has been completed.
  bool waiting_for_init_task_complete_;

  // Callback function used to get the standard upload time.
  base::RepeatingCallback<base::TimeDelta(void)> upload_interval_callback_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_ROTATION_SCHEDULER_H_
