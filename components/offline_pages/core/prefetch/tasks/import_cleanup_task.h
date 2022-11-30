// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_CLEANUP_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_CLEANUP_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchImporter;
class PrefetchStore;

// Reconciliation task for cleaning up database entries that are in IMPORTING
// state. The item can get stuck with IMPORTING state if Chrome is killed
// before the importing completes.
class ImportCleanupTask : public Task {
 public:
  ImportCleanupTask(PrefetchStore* prefetch_store,
                    PrefetchImporter* prefetch_importer);

  ImportCleanupTask(const ImportCleanupTask&) = delete;
  ImportCleanupTask& operator=(const ImportCleanupTask&) = delete;

  ~ImportCleanupTask() override;

 private:
  void Run() override;
  void OnPrefetchItemUpdated(bool row_was_updated);

  raw_ptr<PrefetchStore> prefetch_store_;        // Outlives this class.
  raw_ptr<PrefetchImporter> prefetch_importer_;  // Outlives this class.

  base::WeakPtrFactory<ImportCleanupTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_CLEANUP_TASK_H_
