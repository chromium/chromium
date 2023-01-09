// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_TASK_RUNNER_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_TASK_RUNNER_H_

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/time.h"

namespace openscreen_platform {

class TaskRunner final : public openscreen::TaskRunner {
 public:
  explicit TaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);

  TaskRunner(const TaskRunner&) = delete;
  TaskRunner(TaskRunner&&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
  TaskRunner& operator=(TaskRunner&&) = delete;

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // TaskRunner overrides
  ~TaskRunner() final;
  void PostPackagedTask(openscreen::TaskRunner::Task task) final;
  void PostPackagedTaskWithDelay(openscreen::TaskRunner::Task task,
                                 openscreen::Clock::duration delay) final;
  bool IsRunningOnTaskRunner() final;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_TASK_RUNNER_H_
