// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_DELETE_PAGE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_DELETE_PAGE_TASK_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/task/task.h"

namespace sql {
class Database;
}  // namespace sql

namespace offline_pages {

class OfflinePageMetadataStore;

// Task that deletes pages from the metadata store. It takes the store and
// archive manager for deleting entries from database and file system. Also the
// task needs to be constructed with a DeleteFunction, which defines which pages
// are going to be deleted.
// The caller needs to provide a callback which takes a vector of pages that are
// deleted, along with a DeletePageResult.
// The tasks have to be created by using the static CreateTask* methods.
class DeletePageTask : public Task {
 public:
  struct DeletePageTaskResult;
  using DeletePageTaskCallback =
      base::OnceCallback<void(DeletePageResult,
                              const std::vector<OfflinePageItem>&)>;

  static std::unique_ptr<DeletePageTask> CreateTaskWithCriteria(
      OfflinePageMetadataStore* store,
      const PageCriteria& criteria,
      DeletePageTask::DeletePageTaskCallback callback);

  // Creates a task to delete pages which satisfy |predicate|.
  static std::unique_ptr<DeletePageTask>
  CreateTaskMatchingUrlPredicateForCachedPages(
      OfflinePageMetadataStore* store,
      DeletePageTask::DeletePageTaskCallback callback,
      const UrlPredicate& predicate);

  // Creates a task to delete old pages that have the same url and namespace
  // with |page| to make the number of pages with same url less than the limit
  // defined with the namespace that this |page| belongs to.
  // Returns nullptr if there's no page limit per url of the page's namespace.
  static std::unique_ptr<DeletePageTask> CreateTaskDeletingForPageLimit(
      OfflinePageMetadataStore* store,
      DeletePageTask::DeletePageTaskCallback callback,
      const OfflinePageItem& page);

  DeletePageTask(const DeletePageTask&) = delete;
  DeletePageTask& operator=(const DeletePageTask&) = delete;

  ~DeletePageTask() override;

  // Deletes a single page from the database. This function reads
  // from the database and should be called from within an
  // |SqlStoreBase::Execute()| call.
  static bool DeletePageFromDbSync(int64_t offline_id, sql::Database* db);
  // Deletes all pages with matching offline_ids from the database. Returns
  // false and aborts if a page could not be deleted. This function reads
  // from the database and should be called from within an
  // |SqlStoreBase::Execute()| call.
  static bool DeletePagesFromDbSync(const std::vector<int64_t>& offline_ids,
                                    sql::Database* db);

 private:
  using DeleteFunction =
      base::OnceCallback<DeletePageTaskResult(sql::Database*)>;

  // Task implementation.
  void Run() override;

  // Making the constructor private, in order to use static methods to create
  // tasks.
  DeletePageTask(OfflinePageMetadataStore* store,
                 DeleteFunction func,
                 DeletePageTaskCallback callback);

  void OnDeletePageDone(DeletePageTaskResult result);
  void OnDeleteArchiveFilesDone(
      std::unique_ptr<OfflinePagesUpdateResult> result,
      bool delete_files_result);
  void InformDeletePageDone(DeletePageResult result);

  // The store to delete pages from. Not owned.
  raw_ptr<OfflinePageMetadataStore> store_;
  // The function which will delete pages.
  DeleteFunction func_;
  DeletePageTaskCallback callback_;

  base::WeakPtrFactory<DeletePageTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_DELETE_PAGE_TASK_H_
