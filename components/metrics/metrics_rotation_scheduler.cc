// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_rotation_scheduler.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace metrics {

MetricsRotationScheduler::MetricsRotationScheduler(
    const base::Closure& upload_callback,
    const base::Callback<base::TimeDelta(void)>& upload_interval_callback,
    bool fast_startup_for_testing)
    : MetricsScheduler(upload_callback, fast_startup_for_testing),
      init_task_complete_(false),
      waiting_for_init_task_complete_(false),
      upload_interval_callback_(upload_interval_callback) {}

MetricsRotationScheduler::~MetricsRotationScheduler() {}

void MetricsRotationScheduler::InitTaskComplete() {
  DCHECK(!init_task_complete_);
  init_task_complete_ = true;
  if (waiting_for_init_task_complete_) {
    waiting_for_init_task_complete_ = false;
    TriggerTask();
  } else {
    LogMetricsInitSequence(INIT_TASK_COMPLETED_FIRST);
  }
}

void MetricsRotationScheduler::RotationFinished() {
  TaskDone(upload_interval_callback_.Run());
}

void MetricsRotationScheduler::LogMetricsInitSequence(InitSequence sequence) {
  UMA_HISTOGRAM_ENUMERATION("UMA.InitSequence", sequence,
                            INIT_SEQUENCE_ENUM_SIZE);
}

void MetricsRotationScheduler::TriggerTask() {
  // If the timer fired before the init task has completed, don't trigger the
  // upload yet - wait for the init task to complete and do it then.
  if (!init_task_complete_) {
    LogMetricsInitSequence(TIMER_FIRED_FIRST);
    waiting_for_init_task_complete_ = true;
    return;
  }
  MetricsScheduler::TriggerTask();
}

}  // namespace metrics
