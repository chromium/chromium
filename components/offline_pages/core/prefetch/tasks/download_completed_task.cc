// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/download_completed_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

// Mirrors the OfflinePrefetchDownloadOutcomes metrics enum. Updates
// here should be reflected there and vice versa. New values should be appended
// and existing values never deleted.
enum class DownloadOutcome {
  DOWNLOAD_SUCCEEDED_ITEM_UPDATED,
  DOWNLOAD_FAILED_ITEM_UPDATED,
  DOWNLOAD_SUCCEEDED_ITEM_NOT_FOUND,
  DOWNLOAD_FAILED_ITEM_NOT_FOUND,
  kMaxValue = DOWNLOAD_FAILED_ITEM_NOT_FOUND,
};

DownloadOutcome GetDownloadOutcome(bool successful_download,
                                   bool row_was_updated) {
  if (successful_download) {
    return row_was_updated ? DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_UPDATED
                           : DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_NOT_FOUND;
  }
  return row_was_updated ? DownloadOutcome::DOWNLOAD_FAILED_ITEM_UPDATED
                         : DownloadOutcome::DOWNLOAD_FAILED_ITEM_NOT_FOUND;
}

using UpdateInfo = DownloadCompletedTask::UpdateInfo;

// Updates a prefetch item after its archive was successfully downloaded.
// Returns an UpdateInfo describing the result.
UpdateInfo UpdatePrefetchItemOnDownloadSuccessSync(
    const std::string& guid,
    const base::FilePath& file_path,
    int64_t file_size,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return UpdateInfo();
  // First, grab some data about the page.
  int64_t offline_id;
  ClientId client_id;
  {
    static const char kSql[] =
        "SELECT offline_id, client_namespace, client_id"
        " FROM prefetch_items"
        " WHERE guid = ? AND state = ?";
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, guid);
    statement.BindInt(1, static_cast<int>(PrefetchItemState::DOWNLOADING));
    if (!statement.Step())
      return UpdateInfo();
    offline_id = statement.ColumnInt64(0);
    client_id = ClientId(statement.ColumnString(1), statement.ColumnString(2));
  }

  // Now update the item and return the offline item information.
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, file_path = ?, file_size = ?"
      " WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADED));
  statement.BindString(1, store_utils::ToDatabaseFilePath(file_path));
  statement.BindInt64(2, file_size);
  statement.BindInt64(3, offline_id);
  if (!statement.Run() || db->GetLastChangeCount() != 1 ||
      !transaction.Commit()) {
    return UpdateInfo();
  }
  return UpdateInfo{true, offline_id, client_id};
}

// Updates a prefetch item after its archive failed being downloaded. Returns
// true if the respective row was successfully updated (as normally expected).
UpdateInfo UpdatePrefetchItemOnDownloadErrorSync(const std::string& guid,
                                                 sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, error_code = ?"
      " WHERE guid = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(PrefetchItemErrorCode::DOWNLOAD_ERROR));
  statement.BindString(2, guid);
  statement.BindInt(3, static_cast<int>(PrefetchItemState::DOWNLOADING));

  UpdateInfo info;
  info.success = statement.Run() && db->GetLastChangeCount() > 0;
  return info;
}

}  // namespace

DownloadCompletedTask::DownloadCompletedTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const PrefetchDownloadResult& download_result)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      download_result_(download_result) {
  DCHECK(!download_result_.download_id.empty());
}

DownloadCompletedTask::~DownloadCompletedTask() {}

void DownloadCompletedTask::Run() {
  if (download_result_.success) {
    // Reports downloaded file size in KiB (accepting values up to 100 MiB).
    UMA_HISTOGRAM_COUNTS_100000("OfflinePages.Prefetching.DownloadedFileSize",
                                download_result_.file_size / 1024);
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadSuccessSync,
                       download_result_.download_id, download_result_.file_path,
                       download_result_.file_size),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr(), true),
        UpdateInfo());
  } else {
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadErrorSync,
                       download_result_.download_id),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr(), false),
        UpdateInfo());
  }
}

void DownloadCompletedTask::OnPrefetchItemUpdated(bool successful_download,
                                                  UpdateInfo update_info) {
  // No further action can be done if the database fails to be updated. The
  // cleanup task should eventually kick in to clean this up.
  if (update_info.success) {
    prefetch_dispatcher_->SchedulePipelineProcessing();
  }
  // Kick off thumbnail download if the page was downloaded successfully.
  if (update_info.success && successful_download) {
    prefetch_dispatcher_->ItemDownloaded(update_info.offline_id,
                                         update_info.client_id);
  }

  DownloadOutcome status =
      GetDownloadOutcome(successful_download, update_info.success);
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.Prefetching.DownloadFinishedUpdate",
                            status);

  TaskComplete();
}

}  // namespace offline_pages
