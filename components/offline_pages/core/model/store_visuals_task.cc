// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/store_visuals_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

// Visuals are marked to expire after this delta. Expired visuals are
// eventually deleted if their offline_id does not correspond to an offline
// item. Two days gives us plenty of time so that the prefetched item can be
// imported into the offline item database.
const base::TimeDelta kVisualsExpirationDelta = base::TimeDelta::FromDays(2);

namespace {

bool EnsureRowExistsSync(sql::Database* db,
                         int64_t offline_id,
                         base::Time expiration) {
  static const char kInsertSql[] =
      "INSERT OR IGNORE INTO page_thumbnails"
      " (offline_id,expiration,thumbnail,favicon) VALUES(?,?,x'',x'')";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindInt64(0, offline_id);
  statement.BindInt64(1, store_utils::ToDatabaseTime(expiration));

  return statement.Run();
}

bool StoreThumbnailSync(sql::Database* db,
                        int64_t offline_id,
                        base::Time expiration,
                        const std::string& thumbnail) {
  static const char kUpdateSql[] =
      "UPDATE page_thumbnails SET expiration=?,thumbnail=? WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(expiration));
  statement.BindBlob(1, thumbnail.data(), thumbnail.length());
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

bool StoreFaviconSync(sql::Database* db,
                      int64_t offline_id,
                      base::Time expiration,
                      const std::string& favicon) {
  static const char kUpdateSql[] =
      "UPDATE page_thumbnails SET expiration=?,favicon=? WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(expiration));
  statement.BindBlob(1, favicon.data(), favicon.length());
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

bool StoreVisualsSync(int64_t offline_id,
                      base::Time expiration,
                      const std::string& thumbnail,
                      const std::string& favicon,
                      sql::Database* db) {
  if (expiration == base::Time()) {
    expiration = OfflineTimeNow() + kVisualsExpirationDelta;
  }

  if (!EnsureRowExistsSync(db, offline_id, expiration))
    return false;

  if (!thumbnail.empty() &&
      !StoreThumbnailSync(db, offline_id, expiration, thumbnail))
    return false;

  if (!favicon.empty() &&
      !StoreFaviconSync(db, offline_id, expiration, favicon))
    return false;

  return true;
}

}  // namespace

// static
std::unique_ptr<StoreVisualsTask> StoreVisualsTask::MakeStoreThumbnailTask(
    OfflinePageMetadataStore* store,
    int64_t offline_id,
    std::string thumbnail,
    CompleteCallback callback) {
  auto thumbnail_callback = [](CompleteCallback callback, bool success,
                               std::string thumbnail, std::string favicon) {
    std::move(callback).Run(success, std::move(thumbnail));
  };
  return base::WrapUnique(new StoreVisualsTask(
      store, offline_id, std::move(thumbnail), std::string(),
      base::BindOnce(thumbnail_callback, std::move(callback))));
}

// static
std::unique_ptr<StoreVisualsTask> StoreVisualsTask::MakeStoreFaviconTask(
    OfflinePageMetadataStore* store,
    int64_t offline_id,
    std::string favicon,
    CompleteCallback callback) {
  auto favicon_callback = [](CompleteCallback callback, bool success,
                             std::string thumbnail, std::string favicon) {
    std::move(callback).Run(success, std::move(favicon));
  };
  return base::WrapUnique(new StoreVisualsTask(
      store, offline_id, std::string(), std::move(favicon),
      base::BindOnce(favicon_callback, std::move(callback))));
}

StoreVisualsTask::StoreVisualsTask(OfflinePageMetadataStore* store,
                                   int64_t offline_id,
                                   std::string thumbnail,
                                   std::string favicon,
                                   RowUpdatedCallback complete_callback)
    : store_(store),
      offline_id_(offline_id),
      thumbnail_(std::move(thumbnail)),
      favicon_(std::move(favicon)),
      complete_callback_(std::move(complete_callback)) {}

StoreVisualsTask::~StoreVisualsTask() = default;

void StoreVisualsTask::Run() {
  store_->Execute(base::BindOnce(StoreVisualsSync, offline_id_, expiration_,
                                 thumbnail_, favicon_),
                  base::BindOnce(&StoreVisualsTask::Complete,
                                 weak_ptr_factory_.GetWeakPtr()),
                  false);
}

void StoreVisualsTask::Complete(bool success) {
  TaskComplete();
  std::move(complete_callback_)
      .Run(success, std::move(thumbnail_), std::move(favicon_));
}

}  // namespace offline_pages
