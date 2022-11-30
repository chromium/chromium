// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/task/task.h"

namespace base {
class Time;
}  // namespace base

namespace offline_pages {

class OfflinePageMetadataStore;

// This task is responsible for clearing expired temporary pages from metadata
// store and disk.
// The callback will provide the time when the task starts, how many pages are
// cleared and a ClearStorageResult.
class ClearStorageTask : public Task {
 public:
  // The name of histogram enum is OfflinePagesClearStorageResult.
  enum class ClearStorageResult {
    SUCCESS,                                // Cleared successfully.
    UNNECESSARY,                            // Tried but no page was deleted.
    DEPRECATED_EXPIRE_FAILURE,              // Expiration failed. (DEPRECATED)
    DELETE_FAILURE,                         // Deletion failed.
    DEPRECATED_EXPIRE_AND_DELETE_FAILURES,  // Both expiration and deletion
                                            // failed. (DEPRECATED)
    kMaxValue = DEPRECATED_EXPIRE_AND_DELETE_FAILURES,
  };

  // Callback used when calling ClearPagesIfNeeded.
  // size_t: the number of cleared pages.
  // ClearStorageResult: result of clearing pages in storage.
  typedef base::OnceCallback<void(size_t, ClearStorageResult)>
      ClearStorageCallback;

  ClearStorageTask(OfflinePageMetadataStore* store,
                   ArchiveManager* archive_manager,
                   const base::Time& clearup_time,
                   ClearStorageCallback callback);

  ClearStorageTask(const ClearStorageTask&) = delete;
  ClearStorageTask& operator=(const ClearStorageTask&) = delete;

  ~ClearStorageTask() override;

 private:
  // Task implementation.
  void Run() override;

  void OnGetStorageStatsDone(const ArchiveManager::StorageStats& stats);
  void OnClearPagesDone(std::pair<size_t, DeletePageResult> result);
  void InformClearStorageDone(size_t pages_cleared, ClearStorageResult result);

  // The store containing the pages to be cleared. Not owned.
  raw_ptr<OfflinePageMetadataStore> store_;
  // The archive manager owning the archive directories to delete pages from.
  // Not owned.
  raw_ptr<ArchiveManager> archive_manager_;
  ClearStorageCallback callback_;
  base::Time clearup_time_;

  base::WeakPtrFactory<ClearStorageTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_
