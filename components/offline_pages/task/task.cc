// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "components/offline_pages/task/task_queue.h"

namespace offline_pages {

Task::Task() = default;
Task::~Task() {
  // This may happen when tearing-down the |TaskQueue|.
  DLOG_IF(WARNING,
          state_ == TaskState::kRunning || state_ == TaskState::kSuspended)
      << "Task being destroyed before completion";
}

void Task::Execute(base::OnceClosure complete_callback) {
  DCHECK_EQ(TaskState::kWaiting, state_);

  state_ = TaskState::kRunning;
  task_completion_callback_ = std::move(complete_callback);
  Run();
}

void Task::TaskComplete() {
  DCHECK(state_ == TaskState::kRunning || state_ == TaskState::kSuspended)
      << "Can't complete task in this state: " << static_cast<int>(state_);

  state_ = TaskState::kCompleted;
  if (!task_completion_callback_.is_null())
    std::move(task_completion_callback_).Run();
}

void Task::Suspend() {
  DCHECK_EQ(TaskState::kRunning, state_);
  DCHECK(task_queue_) << "Must be owned by a task queue to suspend.";
  state_ = TaskState::kSuspended;
  task_queue_->SuspendTask(this);
}

void Task::Resume(base::OnceClosure on_resume) {
  DCHECK_EQ(TaskState::kSuspended, state_);
  DCHECK(task_queue_) << "Must be owned by a task queue to resume.";
  task_queue_->ResumeTask(this, std::move(on_resume));
}

}  // namespace offline_pages
