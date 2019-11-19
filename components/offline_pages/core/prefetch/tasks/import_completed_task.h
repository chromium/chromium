// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_COMPLETED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_COMPLETED_TASK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchImporter;
class PrefetchStore;

// Task that responses to the completed import.
class ImportCompletedTask : public Task {
 public:
  ImportCompletedTask(PrefetchDispatcher* prefetch_dispatcher,
                      PrefetchStore* prefetch_store,
                      PrefetchImporter* prefetch_importer,
                      int64_t offline_id,
                      bool success);
  ~ImportCompletedTask() override;

  void Run() override;

 private:
  void OnStateUpdatedToFinished(bool success);

  PrefetchDispatcher* prefetch_dispatcher_;  // Outlives this class.
  PrefetchStore* prefetch_store_;            // Outlives this class.
  PrefetchImporter* prefetch_importer_;      // Outlives this class.
  int64_t offline_id_;
  bool success_;

  base::WeakPtrFactory<ImportCompletedTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImportCompletedTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_COMPLETED_TASK_H_
