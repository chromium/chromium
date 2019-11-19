// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_

#include <vector>

#include "base/macros.h"
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
  ~ImportArchivesTask() override;

  void Run() override;

 private:
  void OnArchivesRetrieved(
      std::unique_ptr<std::vector<PrefetchArchiveInfo>> archive);

  PrefetchStore* prefetch_store_;        // Outlives this class.
  PrefetchImporter* prefetch_importer_;  // Outlives this class.
  PrefetchArchiveInfo archive_;

  base::WeakPtrFactory<ImportArchivesTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImportArchivesTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_IMPORT_ARCHIVES_TASK_H_
