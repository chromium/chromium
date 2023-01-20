// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/unexportable_keys/background_task.h"

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
  // `task`.
  BackgroundTaskImpl(base::OnceCallback<ReturnType()> task,
                     base::OnceCallback<void(ReturnType)> reply)
      : task_(std::move(task)), reply_(std::move(reply)) {
    DCHECK(task_);
    DCHECK(reply_);
  }
  ~BackgroundTaskImpl() override = default;

  // BackgroundTask:
  void Run(scoped_refptr<base::SequencedTaskRunner> background_task_runner,
           base::OnceCallback<void(BackgroundTask* task)> on_complete_callback)
      override {
    background_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(task_),
        std::move(reply_).Then(
            base::BindOnce(std::move(on_complete_callback), this)));
  }

  BackgroundTask::Status GetStatus() const override {
    if (reply_.is_null()) {
      // `reply_` has already been posted to the background task runner.
      return BackgroundTask::Status::kPosted;
    }

    return reply_.IsCancelled() ? BackgroundTask::Status::kCanceled
                                : BackgroundTask::Status::kPending;
  }

 private:
  base::OnceCallback<ReturnType()> task_;
  base::OnceCallback<void(ReturnType)> reply_;
};

}  // namespace unexportable_keys::internal

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_IMPL_H_
