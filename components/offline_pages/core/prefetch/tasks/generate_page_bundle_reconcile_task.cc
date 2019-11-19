// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/generate_page_bundle_reconcile_task.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

struct FetchedUrl {
  FetchedUrl() = default;
  FetchedUrl(int64_t offline_id,
             const std::string& requested_url,
             int generate_bundle_attempts)
      : offline_id_(offline_id),
        requested_url_(requested_url),
        generate_bundle_attempts_(generate_bundle_attempts) {}

  int64_t offline_id_;
  std::string requested_url_;
  int generate_bundle_attempts_;
};

std::vector<FetchedUrl> GetAllUrlsMarkedRequestedSync(sql::Database* db) {
  static const char kSql[] =
      "SELECT offline_id, requested_url, generate_bundle_attempts"
      " FROM prefetch_items"
      " WHERE state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(
      0, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));

  std::vector<FetchedUrl> urls;
  while (statement.Step()) {
    urls.emplace_back(statement.ColumnInt64(0), statement.ColumnString(1),
                      statement.ColumnInt(2));
  }
  return urls;
}

bool MarkUrlFinishedSync(const int64_t offline_id, sql::Database* db) {
  PrefetchItemErrorCode error_code =
      PrefetchItemErrorCode::GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED;
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, error_code = ?"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(error_code));
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

bool MarkUrlForRetrySync(const int64_t offline_id, sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::NEW_REQUEST));
  statement.BindInt64(1, offline_id);
  return statement.Run();
}

bool ReconcileGenerateBundleRequests(
    std::unique_ptr<std::set<std::string>> requested_urls,
    int max_attempts,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  std::vector<FetchedUrl> urls = GetAllUrlsMarkedRequestedSync(db);
  if (urls.empty())
    return false;

  for (const auto& url : urls) {
    // If the url is a part of active request, do nothing.
    if (requested_urls->count(url.requested_url_) > 0)
      continue;

    // Otherwise, update the state to either retry request or error out.
    bool updated = url.generate_bundle_attempts_ >= max_attempts
                       ? MarkUrlFinishedSync(url.offline_id_, db)
                       : MarkUrlForRetrySync(url.offline_id_, db);
    if (!updated)
      return false;
  }

  return transaction.Commit();
}
}  // namespace

// static
const int GeneratePageBundleReconcileTask::kMaxGenerateBundleAttempts = 3;

GeneratePageBundleReconcileTask::GeneratePageBundleReconcileTask(
    PrefetchStore* prefetch_store,
    PrefetchNetworkRequestFactory* request_factory)
    : prefetch_store_(prefetch_store), request_factory_(request_factory) {}

GeneratePageBundleReconcileTask::~GeneratePageBundleReconcileTask() = default;

void GeneratePageBundleReconcileTask::Run() {
  std::unique_ptr<std::set<std::string>> requested_urls =
      request_factory_->GetAllUrlsRequested();
  DCHECK(requested_urls);

  prefetch_store_->Execute(
      base::BindOnce(&ReconcileGenerateBundleRequests,
                     std::move(requested_urls), kMaxGenerateBundleAttempts),
      base::BindOnce(&GeneratePageBundleReconcileTask::FinishedUpdate,
                     weak_factory_.GetWeakPtr()),
      false);
}

void GeneratePageBundleReconcileTask::FinishedUpdate(bool success) {
  // TODO(dimich): report failure/success to UMA.
  TaskComplete();
}

}  // namespace offline_pages
