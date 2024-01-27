// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_long_task_scheduler.h"

#include <string_view>

#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/unexportable_keys/background_task.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"

namespace unexportable_keys {

namespace {

void RecordDurationHistogramWithAndWithoutSuffix(
    const char* base_histogram_name,
    std::string_view suffix,
    base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(base_histogram_name, duration);
  base::UmaHistogramMediumTimes(base::StrCat({base_histogram_name, suffix}),
                                duration);
}

}  // namespace

BackgroundLongTaskScheduler::BackgroundLongTaskScheduler(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)) {
  DCHECK(background_task_runner_);
}

BackgroundLongTaskScheduler::~BackgroundLongTaskScheduler() = default;

void BackgroundLongTaskScheduler::PostTask(
    std::unique_ptr<BackgroundTask> task) {
  TRACE_EVENT("browser",
              "unexportable_keys::BackgroundLongTaskScheduler::PostTask",
              perfetto::Flow::FromPointer(task.get()), "type", task->GetType(),
              "priority", task->GetPriority());
  BackgroundTaskPriority priority = task->GetPriority();
  GetTaskQueueForPriority(priority).push_back(std::move(task));
  // If no task is running, schedule `task` immediately.
  if (!running_task_) {
    MaybeRunNextPendingTask();
  }
}

void BackgroundLongTaskScheduler::OnTaskCompleted(BackgroundTask* task) {
  DCHECK_EQ(running_task_.get(), task);
  TRACE_EVENT("browser",
              "unexportable_keys::BackgroundLongTaskScheduler::OnTaskCompleted",
              perfetto::TerminatingFlow::FromPointer(running_task_.get()));

  std::optional<base::TimeDelta> elapsed_time_since_run =
      task->GetElapsedTimeSinceRun();
  // Task must have been run before being completed.
  CHECK(elapsed_time_since_run.has_value());
  RecordDurationHistogramWithAndWithoutSuffix(
      "Crypto.UnexportableKeys.BackgroundTaskRunDuration",
      GetBackgroundTaskTypeSuffixForHistograms(task->GetType()),
      *elapsed_time_since_run);
  RecordDurationHistogramWithAndWithoutSuffix(
      "Crypto.UnexportableKeys.BackgroundTaskDuration",
      GetBackgroundTaskPrioritySuffixForHistograms(task->GetPriority()),
      task->GetElapsedTimeSinceCreation());

  running_task_.reset();
  MaybeRunNextPendingTask();
}

void BackgroundLongTaskScheduler::MaybeRunNextPendingTask() {
  DCHECK(!running_task_);

  running_task_ = TakeNextPendingTask();
  if (!running_task_) {
    // There is no more pending tasks. Nothing to do.
    return;
  }

  TRACE_EVENT(
      "browser",
      "unexportable_keys::BackgroundLongTaskScheduler::MaybeRunNextPendingTask",
      perfetto::Flow::FromPointer(running_task_.get()));
  RecordDurationHistogramWithAndWithoutSuffix(
      "Crypto.UnexportableKeys.BackgroundTaskQueueWaitDuration",
      GetBackgroundTaskPrioritySuffixForHistograms(
          running_task_->GetPriority()),
      running_task_->GetElapsedTimeSinceCreation());
  running_task_->Run(
      background_task_runner_,
      base::BindOnce(&BackgroundLongTaskScheduler::OnTaskCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

BackgroundLongTaskScheduler::TaskQueue&
BackgroundLongTaskScheduler::GetTaskQueueForPriority(
    BackgroundTaskPriority priority) {
  size_t index = static_cast<size_t>(priority);
  CHECK_LT(index, kNumTaskPriorities);
  return task_queue_by_priority_[index];
}

BackgroundLongTaskScheduler::TaskQueue*
BackgroundLongTaskScheduler::GetHighestPriorityNonEmptyTaskQueue() {
  // Highest priority has the highest value.
  for (int i = kNumTaskPriorities - 1; i >= 0; --i) {
    TaskQueue& queue = task_queue_by_priority_[i];
    if (!queue.empty()) {
      return &queue;
    }
  }
  return nullptr;
}

std::unique_ptr<BackgroundTask>
BackgroundLongTaskScheduler::TakeNextPendingTask() {
  std::unique_ptr<BackgroundTask> next_task;
  while (!next_task) {
    TaskQueue* next_queue = GetHighestPriorityNonEmptyTaskQueue();
    if (!next_queue) {
      return nullptr;
    }

    next_task = std::move(next_queue->front());
    next_queue->pop_front();
    if (next_task->GetStatus() == BackgroundTask::Status::kCanceled) {
      TRACE_EVENT(
          "browser",
          "unexportable_keys::BackgroundLongTaskScheduler::OnTaskCanceled",
          perfetto::TerminatingFlow::FromPointer(next_task.get()));
      // Dismiss a canceled task and try the next one.
      next_task.reset();
    } else {
      DCHECK_EQ(next_task->GetStatus(), BackgroundTask::Status::kPending);
    }
  }
  return next_task;
}

}  // namespace unexportable_keys
