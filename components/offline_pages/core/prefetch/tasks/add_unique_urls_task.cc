// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/add_unique_urls_task.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace offline_pages {

using Result = AddUniqueUrlsTask::Result;

namespace {

std::map<std::string, std::pair<int64_t, PrefetchItemState>>
FindExistingPrefetchItemsInNamespaceSync(sql::Database* db,
                                         const std::string& name_space) {
  static const char kSql[] =
      "SELECT offline_id, state, requested_url FROM prefetch_items"
      " WHERE client_namespace = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, name_space);

  std::map<std::string, std::pair<int64_t, PrefetchItemState>> result;
  while (statement.Step()) {
    result.emplace(
        statement.ColumnString(2),
        std::make_pair(statement.ColumnInt64(0),
                       static_cast<PrefetchItemState>(statement.ColumnInt(1))));
  }

  return result;
}

bool CreatePrefetchItemSync(sql::Database* db,
                            const std::string& name_space,
                            const PrefetchURL& prefetch_url,
                            int64_t now_db_time) {
  static const char kSql[] =
      "INSERT INTO prefetch_items"
      " (offline_id, requested_url, client_namespace, client_id, creation_time,"
      " freshness_time, title)"
      " VALUES"
      " (?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::GenerateOfflineId());
  statement.BindString(1, prefetch_url.url.spec());
  statement.BindString(2, name_space);
  statement.BindString(3, prefetch_url.id);
  statement.BindInt64(4, now_db_time);
  statement.BindInt64(5, now_db_time);
  statement.BindString16(6, prefetch_url.title);

  return statement.Run();
}

// Adds new prefetch item entries to the store using the URLs and client IDs
// from |candidate_prefetch_urls| and the client's |name_space|. Also cleans up
// entries in the Zombie state from the client's |name_space| except for the
// ones whose URL is contained in |candidate_prefetch_urls|.
// Returns the number of added prefecth items.
Result AddUrlsAndCleanupZombiesSync(
    const std::string& name_space,
    const std::vector<PrefetchURL>& candidate_prefetch_urls,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return Result::STORE_ERROR;

  std::map<std::string, std::pair<int64_t, PrefetchItemState>> existing_items =
      FindExistingPrefetchItemsInNamespaceSync(db, name_space);

  int added_row_count = 0;
  base::Time now = base::Time::Now();
  // Insert rows in reverse order to ensure that the beginning of the list has
  // the newest timestamp.  This will cause it to be prefetched first.
  for (auto candidate_iter = candidate_prefetch_urls.rbegin();
       candidate_iter != candidate_prefetch_urls.rend(); ++candidate_iter) {
    PrefetchURL prefetch_url = *candidate_iter;
    auto iter = existing_items.find(prefetch_url.url.spec());
    if (iter == existing_items.end()) {
      if (!CreatePrefetchItemSync(db, name_space, prefetch_url,
                                  store_utils::ToDatabaseTime(now)))
        return Result::STORE_ERROR;  // Transaction rollback.
      added_row_count++;

      // We artificially add a microsecond to ensure that the timestamp is
      // different (and guarantee a particular order when sorting by timestamp).
      now += base::TimeDelta::FromMicroseconds(1);
    } else {
      // Removing from the list of existing items if it was requested again, to
      // prevent it from being removed in the next step.
      existing_items.erase(iter);
    }
  }

  // Purge remaining zombie IDs.
  for (const auto& existing_item : existing_items) {
    if (existing_item.second.second != PrefetchItemState::ZOMBIE)
      continue;
    if (!PrefetchStoreUtils::DeletePrefetchItemByOfflineIdSync(
            db, existing_item.second.first)) {
      return Result::STORE_ERROR;  // Transaction rollback.
    }
  }

  if (!transaction.Commit())
    return Result::STORE_ERROR;  // Transaction rollback.

  UMA_HISTOGRAM_COUNTS_100("OfflinePages.Prefetching.UniqueUrlsAddedCount",
                           added_row_count);
  return added_row_count > 0 ? Result::URLS_ADDED : Result::NOTHING_ADDED;
}
}  // namespace

AddUniqueUrlsTask::AddUniqueUrlsTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      name_space_(name_space),
      prefetch_urls_(prefetch_urls),
      weak_ptr_factory_(this) {
  DCHECK(prefetch_dispatcher_);
  DCHECK(prefetch_store_);
}

AddUniqueUrlsTask::~AddUniqueUrlsTask() {}

void AddUniqueUrlsTask::Run() {
  prefetch_store_->Execute(base::BindOnce(&AddUrlsAndCleanupZombiesSync,
                                          name_space_, prefetch_urls_),
                           base::BindOnce(&AddUniqueUrlsTask::OnUrlsAdded,
                                          weak_ptr_factory_.GetWeakPtr()),
                           Result::STORE_ERROR);
}

void AddUniqueUrlsTask::OnUrlsAdded(Result result) {
  if (result == Result::URLS_ADDED) {
    prefetch_dispatcher_->EnsureTaskScheduled();
    prefetch_dispatcher_->SchedulePipelineProcessing();
  }
  TaskComplete();
}

}  // namespace offline_pages
