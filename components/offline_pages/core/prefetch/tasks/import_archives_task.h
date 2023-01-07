// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchStore;

// Task that attempts to import a downloaded archive to offline page model.
class ImportArchivesTask : public Task {
 public:
  ImportArchivesTask(PrefetchStore* prefetch_store,
                     PrefetchImporter* prefetch_importer);

  ImportArchivesTask(const ImportArchivesTask&) = delete;
  ImportArchivesTask& operator=(const ImportArchivesTask&) = delete;

  ~ImportArchivesTask() override;

 private:
  void Run() override;
  void OnArchivesRetrieved(
      std::unique_ptr<std::vector<PrefetchArchiveInfo>> archive);

  raw_ptr<PrefetchStore> prefetch_store_;        // Outlives this class.
  raw_ptr<PrefetchImporter> prefetch_importer_;  // Outlives this class.
  PrefetchArchiveInfo archive_;

  base::WeakPtrFactory<ImportArchivesTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_
