// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_delayed_task_controller.h"

#include <stddef.h>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"

namespace gcm {

GCMDelayedTaskController::GCMDelayedTaskController() = default;

GCMDelayedTaskController::~GCMDelayedTaskController() = default;

void GCMDelayedTaskController::AddTask(base::OnceClosure task) {
  delayed_tasks_.push_back(std::move(task));
}

void GCMDelayedTaskController::SetReady() {
  if (!ready_) {
    base::UmaHistogramMediumTimes("GCM.DelayedTaskControlledReadyTime",
                                  base::TimeTicks::Now() - time_created_);
  }

  ready_ = true;
  RunTasks();
}

bool GCMDelayedTaskController::CanRunTaskWithoutDelay() const {
  return ready_;
}

void GCMDelayedTaskController::RunTasks() {
  DCHECK(ready_);

  for (size_t i = 0; i < delayed_tasks_.size(); ++i)
    std::move(delayed_tasks_[i]).Run();
  delayed_tasks_.clear();
}

}  // namespace gcm
