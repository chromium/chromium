// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_CLEANUP_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_CLEANUP_TASK_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;

// Reconciliation task for cleaning up database entries that are in DOWNLOADING
// state. This is indeed triggered only when the download service is ready and
// notifies us about the ongoing and completed downloads.
class DownloadCleanupTask : public Task {
 public:
  // Maximum number of attempts to retry a download.
  static const int kMaxDownloadAttempts;

  DownloadCleanupTask(
      PrefetchDispatcher* prefetch_dispatcher,
      PrefetchStore* prefetch_store,
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads);
  ~DownloadCleanupTask() override;

 private:
  void Run() override;
  void OnFinished(bool success);

  PrefetchDispatcher* prefetch_dispatcher_;  // Outlives this class.
  PrefetchStore* prefetch_store_;            // Outlives this class.
  std::set<std::string> outstanding_download_ids_;
  std::map<std::string, std::pair<base::FilePath, int64_t>> success_downloads_;

  base::WeakPtrFactory<DownloadCleanupTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadCleanupTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_CLEANUP_TASK_H_
