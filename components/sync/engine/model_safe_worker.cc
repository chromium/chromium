// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_safe_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"

namespace syncer {

ModelSafeWorker::ModelSafeWorker()
    : work_done_or_abandoned_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
}

ModelSafeWorker::~ModelSafeWorker() {}

void ModelSafeWorker::RequestStop() {
  base::AutoLock auto_lock(lock_);

  // Set stop flag to prevent any *further* WorkCallback from starting to run
  // (note that one may alreay be running).
  stopped_ = true;

  // If no work is running, unblock DoWorkAndWaitUntilDone(). If work is
  // running, it is unsafe to return from DoWorkAndWaitUntilDone().
  // ScopedSignalWorkDoneOrAbandoned will take care of signaling the event when
  // the work is done.
  if (!is_work_running_)
    work_done_or_abandoned_.Signal();
}

SyncerError ModelSafeWorker::DoWorkAndWaitUntilDone(WorkCallback work) {
  {
    // It is important to check |stopped_| and reset |work_done_or_abandoned_|
    // atomically to prevent this race:
    //
    // Thread  Action
    // Sync    Sees that |stopped_| is false.
    // UI      Calls RequestStop(). Signals |work_done_or_abandoned_|.
    // Sync    Resets |work_done_or_abandoned_|.
    //         Waits on |work_done_or_abandoned_| forever since the task may not
    //         run after RequestStop() is called.
    base::AutoLock auto_lock(lock_);
    if (stopped_)
      return SyncerError(SyncerError::CANNOT_DO_WORK);
    DCHECK(!is_work_running_);
    work_done_or_abandoned_.Reset();
  }

  SyncerError error;
  bool did_run = false;
  ScheduleWork(base::BindOnce(&ModelSafeWorker::DoWork, this, std::move(work),
                              base::ScopedClosureRunner(base::BindOnce(
                                  [](scoped_refptr<ModelSafeWorker> worker) {
                                    worker->work_done_or_abandoned_.Signal();
                                  },
                                  base::WrapRefCounted(this))),
                              base::Unretained(&error),
                              base::Unretained(&did_run)));

  // Unblocked when the task runs or is deleted or when RequestStop() is called
  // before the task starts running.
  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    work_done_or_abandoned_.Wait();
  }

  return did_run ? error : SyncerError(SyncerError::CANNOT_DO_WORK);
}

void ModelSafeWorker::DoWork(WorkCallback work,
                             base::ScopedClosureRunner scoped_closure_runner,
                             SyncerError* error,
                             bool* did_run) {
  {
    base::AutoLock auto_lock(lock_);
    if (stopped_)
      return;

    // Set |is_work_running_| to make sure that DoWorkAndWaitUntilDone() doesn't
    // return while |work| is running.
    DCHECK(!is_work_running_);
    is_work_running_ = true;
  }

  *error = std::move(work).Run();
  *did_run = true;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(is_work_running_);
    is_work_running_ = false;
  }
}

}  // namespace syncer
