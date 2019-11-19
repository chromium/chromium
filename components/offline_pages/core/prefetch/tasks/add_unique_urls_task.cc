// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/add_unique_urls_task.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_clock.h"
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

// Returns a map of URL to offline_id for all existing prefetch items.
std::map<std::string, int64_t> GetAllUrlsAndIdsFromNamespaceSync(
    sql::Database* db,
    const std::string& name_space) {
  static const char kSql[] =
      "SELECT requested_url, offline_id FROM prefetch_items"
      " WHERE client_namespace = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, name_space);

  std::map<std::string, int64_t> result;
  while (statement.Step())
    result.emplace(statement.ColumnString(0), statement.ColumnInt64(1));

  return result;
}

bool CreatePrefetchItemSync(sql::Database* db,
                            const std::string& name_space,
                            const PrefetchURL& prefetch_url,
                            int64_t now_db_time) {
  static const char kSql[] =
      "INSERT INTO prefetch_items"
      " (offline_id, requested_url, client_namespace, client_id, creation_time,"
      " freshness_time, title, thumbnail_url, favicon_url, snippet,"
      " attribution)"
      " VALUES"
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::GenerateOfflineId());
  statement.BindString(1, prefetch_url.url.spec());
  statement.BindString(2, name_space);
  statement.BindString(3, prefetch_url.id);
  statement.BindInt64(4, now_db_time);
  statement.BindInt64(5, now_db_time);
  statement.BindString16(6, prefetch_url.title);
  statement.BindString(7, prefetch_url.thumbnail_url.spec());
  statement.BindString(8, prefetch_url.favicon_url.spec());
  statement.BindString(9, prefetch_url.snippet);
  statement.BindString(10, prefetch_url.attribution);

  return statement.Run();
}

bool UpdateItemTimeSync(sql::Database* db,
                        int64_t offline_id,
                        base::Time now_db_time) {
  static const char kSql[] =
      "UPDATE prefetch_items SET"
      " freshness_time=?,creation_time=?"
      " WHERE offline_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(now_db_time));
  statement.BindInt64(1, store_utils::ToDatabaseTime(now_db_time));
  statement.BindInt64(2, offline_id);

  return statement.Run();
}

// Adds new prefetch item entries to the store using the URLs and client IDs
// from |candidate_prefetch_urls| and the client's |name_space|. Returns the
// result of the attempt to add new URLs.
Result AddUniqueUrlsSync(
    const std::string& name_space,
    const std::vector<PrefetchURL>& candidate_prefetch_urls,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return Result::STORE_ERROR;

  std::map<std::string, int64_t> existing_items =
      GetAllUrlsAndIdsFromNamespaceSync(db, name_space);
  std::set<std::string> added_urls;
  int added_row_count = 0;
  base::Time now = OfflineTimeNow();
  // Insert rows in reverse order to ensure that the beginning of the list has
  // the most recent timestamps so that it is prefetched first.
  for (auto candidate_iter = candidate_prefetch_urls.rbegin();
       candidate_iter != candidate_prefetch_urls.rend(); ++candidate_iter) {
    const PrefetchURL& prefetch_url = *candidate_iter;
    const std::string url_spec = prefetch_url.url.spec();
    // Don't add the same URL more than once.
    if (!added_urls.insert(url_spec).second)
      continue;

    auto existing_iter = existing_items.find(url_spec);
    if (existing_iter != existing_items.end()) {
      // An existing item is still being suggested so update its timestamps (and
      // therefore priority).
      if (!UpdateItemTimeSync(db, existing_iter->second, now))
        return Result::STORE_ERROR;  // Transaction rollback.
    } else {
      if (!CreatePrefetchItemSync(db, name_space, prefetch_url,
                                  store_utils::ToDatabaseTime(now))) {
        return Result::STORE_ERROR;  // Transaction rollback.
      }
      added_row_count++;
    }

    // We artificially add a microsecond to ensure that the timestamp is
    // different (and guarantee a particular order when sorting by timestamp).
    now += base::TimeDelta::FromMicroseconds(1);
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
      prefetch_urls_(prefetch_urls) {
  DCHECK(prefetch_dispatcher_);
  DCHECK(prefetch_store_);
}

AddUniqueUrlsTask::~AddUniqueUrlsTask() {}

void AddUniqueUrlsTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&AddUniqueUrlsSync, name_space_, prefetch_urls_),
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
