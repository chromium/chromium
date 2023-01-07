// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_COMPLETED_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_COMPLETED_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class PrefetchDispatcher;
class PrefetchStore;

// Task that responses to the completed download.
class DownloadCompletedTask : public Task {
 public:
  DownloadCompletedTask(PrefetchDispatcher* prefetch_dispatcher,
                        PrefetchStore* prefetch_store,
                        const PrefetchDownloadResult& download_result);

  DownloadCompletedTask(const DownloadCompletedTask&) = delete;
  DownloadCompletedTask& operator=(const DownloadCompletedTask&) = delete;

  ~DownloadCompletedTask() override;

  struct UpdateInfo {
    // True if the row was updated.
    bool success = false;
    int64_t offline_id = 0;
    ClientId client_id;
  };

 private:
  void Run() override;
  void OnPrefetchItemUpdated(bool successful_download, UpdateInfo update_info);

  raw_ptr<PrefetchDispatcher> prefetch_dispatcher_;  // Outlives this class.
  raw_ptr<PrefetchStore> prefetch_store_;            // Outlives this class.
  PrefetchDownloadResult download_result_;

  base::WeakPtrFactory<DownloadCompletedTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_DOWNLOAD_COMPLETED_TASK_H_
