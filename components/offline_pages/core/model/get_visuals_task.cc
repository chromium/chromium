// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/get_visuals_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

std::unique_ptr<OfflinePageVisuals> GetVisualsSync(int64_t offline_id,
                                                   sql::Database* db) {
  std::unique_ptr<OfflinePageVisuals> result;
  static const char kSql[] =
      "SELECT offline_id,expiration,thumbnail,favicon FROM page_thumbnails"
      " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);

  if (!statement.Step()) {
    return result;
  }
  result = std::make_unique<OfflinePageVisuals>();

  result->offline_id = statement.ColumnInt64(0);
  int64_t expiration = statement.ColumnInt64(1);
  result->expiration = store_utils::FromDatabaseTime(expiration);
  if (!statement.ColumnBlobAsString(2, &result->thumbnail))
    result->thumbnail = std::string();

  if (!statement.ColumnBlobAsString(3, &result->favicon))
    result->favicon = std::string();

  return result;
}

}  // namespace

GetVisualsTask::GetVisualsTask(OfflinePageMetadataStore* store,
                               int64_t offline_id,
                               CompleteCallback complete_callback)
    : store_(store),
      offline_id_(offline_id),
      complete_callback_(std::move(complete_callback)) {}

GetVisualsTask::~GetVisualsTask() = default;

void GetVisualsTask::Run() {
  store_->Execute(
      base::BindOnce(GetVisualsSync, std::move(offline_id_)),
      base::BindOnce(&GetVisualsTask::Complete, weak_ptr_factory_.GetWeakPtr()),
      std::unique_ptr<OfflinePageVisuals>());
}

void GetVisualsTask::Complete(std::unique_ptr<OfflinePageVisuals> result) {
  TaskComplete();
  std::move(complete_callback_).Run(std::move(result));
}

}  // namespace offline_pages
