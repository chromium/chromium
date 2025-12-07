// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/unexportable_keys/background_task.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"

namespace unexportable_keys::internal {

// A template class implementing `BackgroundTask`. Background task is
// represented by a `task_` callback with a specific `ReturnType` that is passed
// from the background thread to a `reply_` callback.
template <typename T>
class BackgroundTaskImpl : public BackgroundTask {
 public:
  using ReturnType = T;

  // `task` is a callback that runs on the background thread and returns a
  // value.
  // `reply` is invoked on the posting thread with the return result of
  // `task` and the number or retries it took to compute this result.
  BackgroundTaskImpl(base::RepeatingCallback<ReturnType()> task,
                     base::OnceCallback<void(ReturnType, size_t)> reply,
                     BackgroundTaskPriority priority,
                     BackgroundTaskType type,
                     size_t max_retries)
      : task_(std::move(task)),
        reply_(std::move(reply)),
        priority_(priority),
        type_(type),
        max_retries_(max_retries) {
    DCHECK(task_);
    DCHECK(reply_);
    scheduled_timer_ = base::ElapsedTimer();
  }
  ~BackgroundTaskImpl() override = default;

  // BackgroundTask:
  void Run(scoped_refptr<base::SequencedTaskRunner> background_task_runner,
           base::OnceCallback<void(BackgroundTask* task)> on_complete_callback)
      override {
    CHECK(!result_.has_value());
    run_timer_ = base::ElapsedTimer();
    background_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE, task_,
        base::BindOnce(&BackgroundTaskImpl::OnTaskComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_complete_callback)));
  }

  void ReplyWithResult() override {
    CHECK(result_.has_value());
    std::move(reply_).Run(std::move(result_).value(), retries_);
  }

  void ResetStateBeforeRetry() override {
    result_.reset();
    run_timer_.reset();
    scheduled_timer_ = base::ElapsedTimer();
    ++retries_;
  }

  BackgroundTask::Status GetStatus() const override {
    if (run_timer_.has_value()) {
      // `run_timer_` is started just before posting the task.
      return BackgroundTask::Status::kPosted;
    }

    return reply_.IsCancelled() ? BackgroundTask::Status::kCanceled
                                : BackgroundTask::Status::kPending;
  }

  BackgroundTaskPriority GetPriority() const override { return priority_; }

  BackgroundTaskType GetType() const override { return type_; }

  base::TimeDelta GetElapsedTimeSinceScheduled() const override {
    CHECK(scheduled_timer_.has_value());
    return scheduled_timer_->Elapsed();
  }

  std::optional<base::TimeDelta> GetElapsedTimeSinceRun() const override {
    if (run_timer_.has_value()) {
      return run_timer_->Elapsed();
    }
    return std::nullopt;
  }

  size_t GetRetryCount() const override { return retries_; }

  bool ShouldRetry() const override {
    CHECK(result_.has_value());
    return !reply_.IsCancelled() && retries_ < max_retries_ &&
           ShouldRetryBasedOnResult(*result_);
  }

 protected:
  // Allows subclasses to specify whether the task should be retried based on
  // `result`.
  virtual bool ShouldRetryBasedOnResult(const ReturnType& result) const {
    return false;
  }

 private:
  void OnTaskComplete(
      base::OnceCallback<void(BackgroundTask*)> on_complete_callback,
      ReturnType result) {
    result_ = std::move(result);
    std::move(on_complete_callback).Run(this);
    // `this` might be destroyed after running the callback.
  }

  base::RepeatingCallback<ReturnType()> task_;
  base::OnceCallback<void(ReturnType, size_t)> reply_;

  size_t retries_ = 0;
  std::optional<ReturnType> result_;

  const BackgroundTaskPriority priority_;
  const BackgroundTaskType type_;
  const size_t max_retries_;
  std::optional<base::ElapsedTimer> scheduled_timer_;
  std::optional<base::ElapsedTimer> run_timer_;

  base::WeakPtrFactory<BackgroundTaskImpl> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys::internal

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_
