// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/download_cleanup_task.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

struct DownloadInfo {
  int64_t offline_id;
  std::string guid;
};

std::vector<DownloadInfo> GetAllOutstandingDownloadsSync(sql::Database* db) {
  static const char kSql[] =
      "SELECT offline_id, guid"
      " FROM prefetch_items"
      " WHERE state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADING));

  std::vector<DownloadInfo> downloads;
  while (statement.Step())
    downloads.push_back({statement.ColumnInt64(0), statement.ColumnString(1)});
  return downloads;
}

bool RetryOrExpireDownloadSync(int64_t offline_id,
                               int max_attempts,
                               sql::Database* db) {
  // For all items in DOWNLOADING state:
  // * transit to RECEIVED_BUNDLE state if not exceeding maximum attempts
  // * transit to FINISHED state with error_code set otherwise.
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = CASE WHEN download_initiation_attempts < ? THEN ?"
      "                  ELSE ? END,"
      "     error_code = CASE WHEN download_initiation_attempts < ? "
      "                       THEN error_code ELSE ? END"
      " WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, max_attempts);
  statement.BindInt(1, static_cast<int>(PrefetchItemState::RECEIVED_BUNDLE));
  statement.BindInt(2, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(3, max_attempts);
  statement.BindInt(
      4,
      static_cast<int>(PrefetchItemErrorCode::DOWNLOAD_MAX_ATTEMPTS_REACHED));
  statement.BindInt64(5, offline_id);

  return statement.Run();
}

bool MarkDownloadAsCompletedSync(int64_t offline_id,
                                 const base::FilePath& file_path,
                                 int64_t file_size,
                                 sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, file_path = ?, file_size = ?"
      " WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADED));
  statement.BindString(1, store_utils::ToDatabaseFilePath(file_path));
  statement.BindInt64(2, file_size);
  statement.BindInt64(3, offline_id);

  return statement.Run();
}

bool CleanupDownloadsSync(
    int max_attempts,
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // TODO(carlosk): add UMA to track any extraordinary conditions detected here.
  std::vector<DownloadInfo> outstanding_prefetch_downloads =
      GetAllOutstandingDownloadsSync(db);
  if (outstanding_prefetch_downloads.empty())
    return true;

  for (const auto& outstanding_prefetch_download :
       outstanding_prefetch_downloads) {
    std::string outstanding_prefetch_download_id =
        outstanding_prefetch_download.guid;

    // If the outstanding prefetch download has already completed successfully
    // per the download system, mark it as download completed.
    const auto& success_download_iter =
        success_downloads.find(outstanding_prefetch_download_id);
    if (success_download_iter != success_downloads.end()) {
      if (!MarkDownloadAsCompletedSync(
              outstanding_prefetch_download.offline_id,
              success_download_iter->second.first,   // file path
              success_download_iter->second.second,  // file size
              db)) {
        return false;
      }
      continue;
    }

    // If the download is in progress, no change.
    if (outstanding_download_ids.find(outstanding_prefetch_download_id) !=
        outstanding_download_ids.end()) {
      continue;
    }

    // Otherwise, update the state to either retry the opeation or error out.
    if (!RetryOrExpireDownloadSync(outstanding_prefetch_download.offline_id,
                                   max_attempts, db)) {
      return false;
    }
  }

  return transaction.Commit();
}

}  // namespace

// static
const int DownloadCleanupTask::kMaxDownloadAttempts = 3;

DownloadCleanupTask::DownloadCleanupTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      outstanding_download_ids_(outstanding_download_ids),
      success_downloads_(success_downloads) {}

DownloadCleanupTask::~DownloadCleanupTask() {}

void DownloadCleanupTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&CleanupDownloadsSync, kMaxDownloadAttempts,
                     outstanding_download_ids_, success_downloads_),
      base::BindOnce(&DownloadCleanupTask::OnFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void DownloadCleanupTask::OnFinished(bool success) {
  if (success)
    prefetch_dispatcher_->SchedulePipelineProcessing();

  TaskComplete();
}

}  // namespace offline_pages
