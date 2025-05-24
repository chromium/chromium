// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/submittable_executor.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"

namespace nearby::chrome {

SubmittableExecutor::SubmittableExecutor(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

SubmittableExecutor::~SubmittableExecutor() {
  {
    base::AutoLock al(lock_);
    is_shut_down_ = true;
    if (num_incomplete_tasks_ == 0)
      last_task_completed_.Signal();
  }

  // Block until all pending tasks are finished.
  last_task_completed_.Wait();

  // Grab the lock to ensure that RunTask() has returned.
  base::AutoLock al(lock_);
  CHECK_EQ(num_incomplete_tasks_, 0);
}

// Once called, this method will prevent any future calls to Submit() or
// Execute() from posting additional tasks. Previously posted asks will be
// allowed to complete normally.
void SubmittableExecutor::Shutdown() {
  base::AutoLock al(lock_);
  is_shut_down_ = true;
}

// Posts the given |runnable| and returns true immediately. If Shutdown() has
// been called, this method will return false.
bool SubmittableExecutor::DoSubmit(Runnable&& runnable) {
  base::AutoLock al(lock_);
  if (is_shut_down_)
    return false;

  ++num_incomplete_tasks_;
  return task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SubmittableExecutor::RunTask,
                                base::Unretained(this), std::move(runnable)));
}

// Posts the given |runnable| and returns immediately. If Shutdown() has been
// called, this method will do nothing.
void SubmittableExecutor::Execute(Runnable&& runnable) {
  base::AutoLock al(lock_);
  if (is_shut_down_)
    return;

  ++num_incomplete_tasks_;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SubmittableExecutor::RunTask,
                                base::Unretained(this), std::move(runnable)));
}

void SubmittableExecutor::RunTask(Runnable&& runnable) {
  {
    // The Nearby Connections library relies on many long running thread tasks
    // which do not meet the usual usage pattern of a task on the chrome thread
    // pool (short lived and non-blocking). Without WILL_BLOCK we end up thread
    // starving tasks when multiple clients are using Nearby Connections at the
    // same time because the thread pool by default is not big enough. By
    // scoping this task as WILL_BLOCK we are letting the thread pool know that
    // it should allocate an additional thread so this task does not starve
    // other tasks if it does block and/or is long lived. See b/185628066 for
    // examples of this starvation happening before this change.
    base::ScopedBlockingCall blocking_call{FROM_HERE,
                                           base::BlockingType::WILL_BLOCK};
    // base::ScopedAllowBaseSyncPrimitives is required as code inside the
    // runnable uses blocking primitive, which lives outside Chrome.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    runnable();
  }

  base::AutoLock al(lock_);
  CHECK_GE(num_incomplete_tasks_, 1);
  if (--num_incomplete_tasks_ == 0 && is_shut_down_)
    last_task_completed_.Signal();
}

}  // namespace nearby::chrome
