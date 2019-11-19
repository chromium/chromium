// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_STALE_ENTRY_FINALIZER_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_STALE_ENTRY_FINALIZER_TASK_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;

// Reconciliation task responsible for finalizing entries for which their
// freshness date are past the limits specific to each pipeline bucket. Entries
// considered stale are moved to the "finished" state and have their error code
// column set to the PrefetchItemErrorCode value that identifies the bucket they
// were at.
// It also handles items in the the "zombie" state which are deleted once
// considered expired after a set amount of time.
// NOTE: This task is run periodically as reconciliation task and from some
// event handlers. As such, it must not cause network operations nor cause
// 'progress' in the pipeline that would trigger other tasks.
class StaleEntryFinalizerTask : public Task {
 public:
  enum class Result { NO_MORE_WORK, MORE_WORK_NEEDED };

  StaleEntryFinalizerTask(PrefetchDispatcher* prefetch_dispatcher,
                          PrefetchStore* prefetch_store);
  ~StaleEntryFinalizerTask() override;

  void Run() override;

  // Will be set to true upon after an error-free run.
  Result final_status() const { return final_status_; }

 private:
  void OnFinished(Result result);

  // Not owned.
  PrefetchDispatcher* prefetch_dispatcher_;

  // Prefetch store to execute against. Not owned.
  PrefetchStore* prefetch_store_;

  Result final_status_ = Result::NO_MORE_WORK;

  base::WeakPtrFactory<StaleEntryFinalizerTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StaleEntryFinalizerTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_STALE_ENTRY_FINALIZER_TASK_H_
