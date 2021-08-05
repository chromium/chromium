// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_PERSISTENT_PAGE_CONSISTENCY_CHECK_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_PERSISTENT_PAGE_CONSISTENCY_CHECK_TASK_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class ArchiveManager;
class OfflinePageMetadataStore;

// This task is responsible for checking consistency of persistent pages, mark
// the expired ones with the file missing time and recover the previously
// missing entries back to normal.
class PersistentPageConsistencyCheckTask : public Task {
 public:
  using PersistentPageConsistencyCheckCallback = base::OnceCallback<void(
      bool success,
      const std::vector<PublishedArchiveId>& ids_of_deleted_pages)>;

  struct CheckResult {
    CheckResult();
    CheckResult(SyncOperationResult result,
                const std::vector<PublishedArchiveId>& ids_of_deleted_pages);
    CheckResult(const CheckResult& other);
    CheckResult& operator=(const CheckResult& other);
    ~CheckResult();

    SyncOperationResult result;
    std::vector<PublishedArchiveId> ids_of_deleted_pages;
  };

  PersistentPageConsistencyCheckTask(
      OfflinePageMetadataStore* store,
      ArchiveManager* archive_manager,
      base::Time check_time,
      PersistentPageConsistencyCheckCallback callback);
  ~PersistentPageConsistencyCheckTask() override;

 private:
  // Task implementation:
  void Run() override;

  void OnPersistentPageConsistencyCheckDone(CheckResult result);

  // The store containing the offline pages. Not owned.
  OfflinePageMetadataStore* store_;
  // The archive manager storing archive directories. Not owned.
  ArchiveManager* archive_manager_;
  base::Time check_time_;
  // The callback for the task.
  PersistentPageConsistencyCheckCallback callback_;

  base::WeakPtrFactory<PersistentPageConsistencyCheckTask> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(PersistentPageConsistencyCheckTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_PERSISTENT_PAGE_CONSISTENCY_CHECK_TASK_H_
