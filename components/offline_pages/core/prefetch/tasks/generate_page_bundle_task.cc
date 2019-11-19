// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/generate_page_bundle_task.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

// A wrapper for two vectors, one with string URLs and one with offline and
// client ids pairs, each holding data for the same set of prefetch items.
struct GeneratePageBundleTask::UrlAndIds {
  std::vector<std::string> urls;
  PrefetchDispatcher::IdsVector ids;
};

namespace {

using UrlAndIds = GeneratePageBundleTask::UrlAndIds;

// Temporary storage for Urls metadata fetched from the storage.
struct FetchedUrl {
  FetchedUrl() = default;
  FetchedUrl(int64_t offline_id,
             ClientId client_id,
             const std::string& requested_url,
             int generate_bundle_attempts)
      : offline_id(offline_id),
        client_id(client_id),
        requested_url(requested_url),
        generate_bundle_attempts(generate_bundle_attempts) {}

  int64_t offline_id;
  ClientId client_id;
  std::string requested_url;
  int generate_bundle_attempts;
};

// This is maximum URLs that Offline Page Service can take in one request.
const int kMaxUrlsToSend = 100;

bool UpdateStateSync(sql::Database* db, const int64_t offline_id) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?,"
      "     freshness_time = CASE WHEN generate_bundle_attempts = 0 THEN ?"
      "                           ELSE freshness_time END,"
      "     generate_bundle_attempts = generate_bundle_attempts + 1"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(
      0, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));
  statement.BindInt64(1, store_utils::ToDatabaseTime(OfflineTimeNow()));
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

std::unique_ptr<std::vector<FetchedUrl>> FetchUrlsSync(sql::Database* db) {
  static const char kSql[] =
      "SELECT offline_id, client_namespace, client_id, requested_url,"
      "       generate_bundle_attempts"
      " FROM prefetch_items"
      " WHERE state = ?"
      " ORDER BY creation_time DESC";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::NEW_REQUEST));

  auto urls = std::make_unique<std::vector<FetchedUrl>>();
  while (statement.Step()) {
    urls->push_back(FetchedUrl(
        // offline_id
        statement.ColumnInt64(0),
        // client_id
        {statement.ColumnString(1), statement.ColumnString(2)},
        // requested_url
        statement.ColumnString(3),
        // generate_bundle_attempts
        statement.ColumnInt(4)));
  }

  return urls;
}

bool MarkUrlFinishedWithError(sql::Database* db, const FetchedUrl& url) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1,
                    static_cast<int>(PrefetchItemErrorCode::TOO_MANY_NEW_URLS));
  statement.BindInt64(2, url.offline_id);
  return statement.Run();
}

std::unique_ptr<UrlAndIds> SelectUrlsToPrefetchSync(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return nullptr;

  auto urls = FetchUrlsSync(db);
  if (!urls || urls->empty())
    return nullptr;

  // If we've got more than kMaxUrlsToSend URLs, mark the extra ones FINISHED
  // and remove them from the list.
  if (urls->size() > kMaxUrlsToSend) {
    for (size_t index = kMaxUrlsToSend; index < urls->size(); ++index) {
      if (!MarkUrlFinishedWithError(db, urls->at(index)))
        return nullptr;
    }
    urls->resize(kMaxUrlsToSend);
  }

  auto url_and_ids = std::make_unique<UrlAndIds>();
  for (const auto& url : *urls) {
    if (!UpdateStateSync(db, url.offline_id))
      return nullptr;
    url_and_ids->urls.push_back(std::move(url.requested_url));
    url_and_ids->ids.push_back({url.offline_id, std::move(url.client_id)});
  }

  if (!transaction.Commit())
    return nullptr;

  return url_and_ids;
}
}  // namespace

GeneratePageBundleTask::GeneratePageBundleTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const std::string& gcm_token,
    PrefetchNetworkRequestFactory* request_factory,
    PrefetchRequestFinishedCallback callback)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      gcm_token_(gcm_token),
      request_factory_(request_factory),
      callback_(std::move(callback)) {}

GeneratePageBundleTask::~GeneratePageBundleTask() {}

void GeneratePageBundleTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&SelectUrlsToPrefetchSync),
      base::BindOnce(&GeneratePageBundleTask::StartGeneratePageBundle,
                     weak_factory_.GetWeakPtr()),
      std::unique_ptr<UrlAndIds>());
}

void GeneratePageBundleTask::StartGeneratePageBundle(
    std::unique_ptr<UrlAndIds> url_and_ids) {
  if (!url_and_ids) {
    TaskComplete();
    return;
  }
  DCHECK(!url_and_ids->urls.empty());
  DCHECK_EQ(url_and_ids->urls.size(), url_and_ids->ids.size());

  request_factory_->MakeGeneratePageBundleRequest(url_and_ids->urls, gcm_token_,
                                                  std::move(callback_));
  prefetch_dispatcher_->GeneratePageBundleRequested(
      std::make_unique<PrefetchDispatcher::IdsVector>(
          std::move(url_and_ids->ids)));
  TaskComplete();
}

}  // namespace offline_pages
