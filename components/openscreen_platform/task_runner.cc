// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/task_runner.h"

#include <chrono>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace openscreen_platform {

using openscreen::Clock;
using Task = openscreen::TaskRunner::Task;

namespace {
void ExecuteTask(Task task) {
  task();
}
}  // namespace

TaskRunner::TaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

TaskRunner::~TaskRunner() = default;

void TaskRunner::PostPackagedTask(Task task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(ExecuteTask, std::move(task)));
}

void TaskRunner::PostPackagedTaskWithDelay(Task task, Clock::duration delay) {
  auto time_delta = base::Microseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(delay).count());
  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(ExecuteTask, std::move(task)), time_delta);
}

bool TaskRunner::IsRunningOnTaskRunner() {
  return task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace openscreen_platform
