// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/delete_page_task.h"

#include <iterator>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

struct DeletePageTask::DeletePageTaskResult {
  DeletePageTaskResult() = default;
  DeletePageTaskResult(DeletePageResult result,
                       const std::vector<OfflinePageItem>& deleted_pages)
      : result(result), deleted_pages(deleted_pages) {}
  DeletePageTaskResult(DeletePageTaskResult&& other) = default;
  DeletePageTaskResult(const DeletePageTaskResult& other) = delete;
  ~DeletePageTaskResult() = default;

  DeletePageResult result;
  std::vector<OfflinePageItem> deleted_pages;
};
using DeletePageTaskResult = DeletePageTask::DeletePageTaskResult;

namespace {

void ReportDeletePageHistograms(
    const std::vector<OfflinePageItem>& deleted_pages) {
  const int max_minutes = base::TimeDelta::FromDays(365).InMinutes();
  base::Time delete_time = OfflineTimeNow();
  for (const auto& item : deleted_pages) {
    base::UmaHistogramCustomCounts(
        model_utils::AddHistogramSuffix(item.client_id.name_space,
                                        "OfflinePages.PageLifetime"),
        (delete_time - item.creation_time).InMinutes(), 1, max_minutes, 100);
    base::UmaHistogramCustomCounts(
        model_utils::AddHistogramSuffix(item.client_id.name_space,
                                        "OfflinePages.AccessCount"),
        item.access_count, 1, 1000000, 50);
  }
}

bool DeleteArchiveSync(const base::FilePath& file_path) {
  // Delete the file only, |false| for recursive.
  return base::DeleteFile(file_path, false);
}

// Deletes pages. This will return a DeletePageTaskResult which contains the
// deleted pages (which are successfully deleted from the disk and the store)
// and a DeletePageResult. For each page to be deleted, the deletion will delete
// the archive file first, then database entry, in order to avoid the potential
// issue of leaving archive files behind (and they may be imported later).
// Since the database entry will only be deleted while the associated archive
// file is deleted successfully, there will be no such issue.
DeletePageTaskResult DeletePagesSync(
    sql::Database* db,
    std::vector<OfflinePageItem> pages_to_delete) {
  std::vector<OfflinePageItem> deleted_pages;

  // If there's no page to delete, return an empty list with SUCCESS.
  if (pages_to_delete.size() == 0)
    return {DeletePageResult::SUCCESS, deleted_pages};

  ReportDeletePageHistograms(pages_to_delete);

  bool any_archive_deleted = false;
  for (auto& item : pages_to_delete) {
    if (DeleteArchiveSync(item.file_path)) {
      any_archive_deleted = true;
      if (DeletePageTask::DeletePageFromDbSync(item.offline_id, db))
        deleted_pages.push_back(std::move(item));
    }
  }
  // If there're no files deleted, return DEVICE_FAILURE.
  if (!any_archive_deleted)
    return {DeletePageResult::DEVICE_FAILURE, std::move(deleted_pages)};

  return {DeletePageResult::SUCCESS, std::move(deleted_pages)};
}

DeletePageTaskResult DeletePagesWithCriteria(
    const PageCriteria& criteria,
    sql::Database* db) {
  // If you create a transaction but dont Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return {DeletePageResult::STORE_FAILURE, {}};

  GetPagesTask::ReadResult read_result =
      GetPagesTask::ReadPagesWithCriteriaSync(criteria, db);
  if (!read_result.success)
    return {DeletePageResult::STORE_FAILURE, {}};

  DeletePageTaskResult result =
      DeletePagesSync(db, std::move(read_result.pages));

  if (!transaction.Commit())
    return {DeletePageResult::STORE_FAILURE, {}};
  return result;
}

// Deletes all but |limit| pages that match |criteria|, in the order specified
// by |criteria|.
DeletePageTaskResult DeletePagesForPageLimit(
    const PageCriteria& criteria,
    size_t limit,
    sql::Database* db) {
  // If the namespace can have unlimited pages per url, just return success. In
  // practice, we shouldn't hit this condition.
  if (limit == kUnlimitedPages) {
    DLOG(ERROR) << "DeletePagesForPageLimit called with unlimited limit";
    return {DeletePageResult::SUCCESS, {}};
  }

  // If you create a transaction but dont Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return {DeletePageResult::STORE_FAILURE, {}};

  GetPagesTask::ReadResult read_result =
      GetPagesTask::ReadPagesWithCriteriaSync(criteria, db);
  if (!read_result.success)
    return {DeletePageResult::STORE_FAILURE, {}};

  if (read_result.pages.size() > limit)
    read_result.pages.resize(read_result.pages.size() - limit);
  else
    read_result.pages.clear();

  DeletePageTaskResult result =
      DeletePagesSync(db, std::move(read_result.pages));

  if (!transaction.Commit())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  return result;
}

}  // namespace

// static
std::unique_ptr<DeletePageTask> DeletePageTask::CreateTaskWithCriteria(
    OfflinePageMetadataStore* store,
    const PageCriteria& criteria,
    DeletePageTask::DeletePageTaskCallback callback) {
  return std::unique_ptr<DeletePageTask>(new DeletePageTask(
      store, base::BindOnce(&DeletePagesWithCriteria, criteria),
      std::move(callback)));
}

// static
std::unique_ptr<DeletePageTask>
DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
    OfflinePageMetadataStore* store,
    DeletePageTask::DeletePageTaskCallback callback,
    const UrlPredicate& predicate) {
  PageCriteria criteria;
  criteria.lifetime_type = LifetimeType::TEMPORARY;
  criteria.additional_criteria = base::BindRepeating(
      [](const UrlPredicate& predicate, const OfflinePageItem& item) {
        return predicate.Run(item.url);
      },
      predicate);
  return CreateTaskWithCriteria(store, criteria, std::move(callback));
}

// static
std::unique_ptr<DeletePageTask> DeletePageTask::CreateTaskDeletingForPageLimit(
    OfflinePageMetadataStore* store,
    DeletePageTask::DeletePageTaskCallback callback,
    const OfflinePageItem& page) {
  std::string name_space = page.client_id.name_space;
  size_t limit = GetPolicy(name_space).pages_allowed_per_url;
  PageCriteria criteria;
  criteria.url = page.url;
  criteria.client_namespaces = std::vector<std::string>{name_space};
  // Sorting is important here. DeletePagesForPageLimit will delete the results
  // in order, leaving only the last |limit| pages.
  criteria.result_order = PageCriteria::kAscendingAccessTime;
  return base::WrapUnique(new DeletePageTask(
      store, base::BindOnce(&DeletePagesForPageLimit, criteria, limit),
      std::move(callback)));
}

DeletePageTask::DeletePageTask(OfflinePageMetadataStore* store,
                               DeleteFunction func,
                               DeletePageTaskCallback callback)
    : store_(store), func_(std::move(func)), callback_(std::move(callback)) {
  DCHECK(store_);
  DCHECK(!callback_.is_null());
}

DeletePageTask::~DeletePageTask() {}

void DeletePageTask::Run() {
  store_->Execute(std::move(func_),
                  base::BindOnce(&DeletePageTask::OnDeletePageDone,
                                 weak_ptr_factory_.GetWeakPtr()),
                  DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {}));
}

void DeletePageTask::OnDeletePageDone(DeletePageTaskResult result) {
  std::move(callback_).Run(result.result, std::move(result.deleted_pages));
  TaskComplete();
}

// static
bool DeletePageTask::DeletePageFromDbSync(int64_t offline_id,
                                          sql::Database* db) {
  static const char kSql[] = "DELETE FROM offlinepages_v1 WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

bool DeletePageTask::DeletePagesFromDbSync(
    const std::vector<int64_t>& offline_ids,
    sql::Database* db) {
  for (const auto& offline_id : offline_ids) {
    if (!DeletePageTask::DeletePageFromDbSync(offline_id, db))
      return false;
  }
  return true;
}

}  // namespace offline_pages
