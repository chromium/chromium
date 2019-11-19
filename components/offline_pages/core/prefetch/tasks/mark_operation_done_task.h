// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_MARK_OPERATION_DONE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_MARK_OPERATION_DONE_TASK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;

// Event Handler Task that responds to GCM messages and marks the corresponding
// operation as finished in the DB.  This task then requests action tasks if any
// urls were affected.
class MarkOperationDoneTask : public Task {
 public:
  // StoreResult represents the overall status of executing the command in the
  // SQL store.
  enum class StoreResult {
    UNFINISHED = 0,
    UPDATED,
    STORE_ERROR,
  };

  // TaskResult includes both a StoreResult and the number of rows that were
  // affected by a successful SQL command.
  using TaskResult = std::pair<StoreResult, int64_t>;

  // TODO(dewittj): Schedule action tasks via "SchedulePipelineProcessing" if we
  // are in a background task.
  MarkOperationDoneTask(PrefetchDispatcher* prefetch_dispatcher,
                        PrefetchStore* prefetch_store,
                        const std::string& operation_name);
  ~MarkOperationDoneTask() override;

  // Task implementation.
  void Run() override;

  StoreResult store_result() const { return std::get<0>(result_); }

  // Number of rows changed, or -1 if there was a store error (or not yet
  // finished).
  int64_t change_count() const { return std::get<1>(result_); }

 private:
  void MarkOperationDone(int updated_entry_count);
  void Done(TaskResult result);

  PrefetchDispatcher* prefetch_dispatcher_;
  PrefetchStore* prefetch_store_;
  std::string operation_name_;
  TaskResult result_ = std::make_pair(StoreResult::UNFINISHED, -1);

  base::WeakPtrFactory<MarkOperationDoneTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MarkOperationDoneTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_MARK_OPERATION_DONE_TASK_H_
