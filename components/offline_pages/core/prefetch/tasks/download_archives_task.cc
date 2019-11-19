// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/download_archives_task.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_downloader_quota.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

using DownloadItem = DownloadArchivesTask::DownloadItem;
using ItemsToDownload = DownloadArchivesTask::ItemsToDownload;

ItemsToDownload FindItemsReadyForDownload(sql::Database* db) {
  static const char kSql[] =
      "SELECT offline_id,archive_body_name,operation_name,"
      "archive_body_length"
      " FROM prefetch_items"
      " WHERE state=?"
      " ORDER BY creation_time DESC";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::RECEIVED_BUNDLE));

  ItemsToDownload items_to_download;
  while (statement.Step()) {
    DownloadItem item;
    item.offline_id = statement.ColumnInt64(0);
    item.archive_body_name = statement.ColumnString(1);
    item.operation_name = statement.ColumnString(2);
    item.archive_body_length = statement.ColumnInt64(3);
    items_to_download.push_back(std::move(item));
  }

  return items_to_download;
}

std::unique_ptr<int> CountDownloadsInProgress(sql::Database* db) {
  static const char kSql[] =
      "SELECT COUNT(offline_id) FROM prefetch_items WHERE state=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADING));
  if (!statement.Step())
    return nullptr;
  return std::make_unique<int>(statement.ColumnInt(0));
}

bool MarkItemAsDownloading(sql::Database* db,
                           int64_t offline_id,
                           const std::string& guid) {
  // Code below only changes freshness time once, when the archive download is
  // attempted for the first time. We don't want to perpetuate the lifetime of
  // the item longer than that, if we keep on retrying it.
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state=?,guid=?,"
      "freshness_time=CASE WHEN download_initiation_attempts=0 THEN ? "
      "ELSE freshness_time END,"
      "download_initiation_attempts=download_initiation_attempts+1"
      " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADING));
  statement.BindString(1, guid);
  statement.BindInt64(2, store_utils::ToDatabaseTime(OfflineTimeNow()));
  statement.BindInt64(3, offline_id);
  return statement.Run();
}

std::unique_ptr<ItemsToDownload> SelectAndMarkItemsForDownloadSync(
    bool is_limitless_prefetching_enabled,
    int max_concurrent_downloads,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return nullptr;

  // Get current count of concurrent downloads and bail early if we are already
  // downloading more than we can.
  std::unique_ptr<int> concurrent_downloads(CountDownloadsInProgress(db));
  if (!concurrent_downloads ||
      *concurrent_downloads >= max_concurrent_downloads) {
    return nullptr;
  }

  PrefetchDownloaderQuota downloader_quota(db, OfflineClock());
  int64_t available_quota = downloader_quota.GetAvailableQuotaBytes();
  if (available_quota <= 0 && !is_limitless_prefetching_enabled)
    return nullptr;

  ItemsToDownload ready_items = FindItemsReadyForDownload(db);
  if (ready_items.empty())
    return nullptr;

  // Below implementation is a greedy algorithm that selects the next item we
  // can download without quota violation and maximum concurrent downloads
  // violation, as ordered by the |FindItemsReadyForDownload| function.
  auto items_to_download = std::make_unique<ItemsToDownload>();
  for (auto& ready_item : ready_items) {
    // Concurrent downloads check.
    if (*concurrent_downloads >= max_concurrent_downloads)
      break;

    // Quota check. Skips all items that violate quota if limitless prefetching
    // is disabled. If it is enabled then always proceed but still decrement the
    // quota allowing it to become negative.
    if (ready_item.archive_body_length > available_quota &&
        !is_limitless_prefetching_enabled) {
      continue;
    }
    available_quota -= ready_item.archive_body_length;

    // Explicitly not reusing the GUID from the last archive download attempt
    // here.
    std::string guid = base::GenerateGUID();
    if (!MarkItemAsDownloading(db, ready_item.offline_id, guid))
      return nullptr;
    ready_item.guid = guid;
    items_to_download->emplace_back(ready_item);
    ++(*concurrent_downloads);
  }

  // Write new remaining quota, limited to a minimum of 0.
  available_quota = available_quota < 0 ? 0 : available_quota;
  if (!downloader_quota.SetAvailableQuotaBytes(available_quota))
    return nullptr;

  if (!transaction.Commit())
    return nullptr;

  return items_to_download;
}

}  // namespace

DownloadArchivesTask::DownloadItem::DownloadItem() = default;
DownloadArchivesTask::DownloadItem::DownloadItem(const DownloadItem& other) =
    default;

DownloadArchivesTask::DownloadArchivesTask(
    PrefetchStore* prefetch_store,
    PrefetchDownloader* prefetch_downloader,
    PrefService* prefs)
    : prefetch_store_(prefetch_store),
      prefetch_downloader_(prefetch_downloader),
      prefs_(prefs) {
  DCHECK(prefetch_store_);
  DCHECK(prefetch_downloader_);
  DCHECK(prefs_);
}

DownloadArchivesTask::~DownloadArchivesTask() = default;

void DownloadArchivesTask::Run() {
  // Don't schedule any downloads if the download service can't be used at all.
  if (prefetch_downloader_->IsDownloadServiceUnavailable()) {
    TaskComplete();
    return;
  }
  const bool is_limitless_prefetching_enabled =
      prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs_);
  const int max_concurrent_downloads =
      is_limitless_prefetching_enabled
          ? prefetch_downloader_->GetMaxConcurrentDownloads()
          : DownloadArchivesTask::kMaxConcurrentDownloads;

  prefetch_store_->Execute(
      base::BindOnce(&SelectAndMarkItemsForDownloadSync,
                     is_limitless_prefetching_enabled,
                     max_concurrent_downloads),
      base::BindOnce(&DownloadArchivesTask::SendItemsToPrefetchDownloader,
                     weak_ptr_factory_.GetWeakPtr()),
      std::unique_ptr<ItemsToDownload>());
}

void DownloadArchivesTask::SendItemsToPrefetchDownloader(
    std::unique_ptr<ItemsToDownload> items_to_download) {
  if (items_to_download) {
    for (const auto& download_item : *items_to_download) {
      prefetch_downloader_->StartDownload(download_item.guid,
                                          download_item.archive_body_name,
                                          download_item.operation_name);
      // Reports expected archive size in KiB (accepting values up to 100 MiB).
      UMA_HISTOGRAM_COUNTS_100000(
          "OfflinePages.Prefetching.DownloadExpectedFileSize",
          download_item.archive_body_length / 1024);
    }
  }

  TaskComplete();
}

}  // namespace offline_pages
