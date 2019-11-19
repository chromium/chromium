// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/cleanup_visuals_task.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {
typedef base::OnceCallback<void(CleanupVisualsTask::Result)> ResultCallback;

CleanupVisualsTask::Result CleanupVisualsSync(base::Time now,
                                              sql::Database* db) {
  static const char kSql[] =
      "DELETE FROM page_thumbnails "
      "WHERE offline_id IN ("
      "  SELECT pt.offline_id from page_thumbnails pt"
      "  LEFT OUTER JOIN offlinepages_v1 op"
      "  ON pt.offline_id = op.offline_id "
      "  WHERE op.offline_id IS NULL "
      "  AND pt.expiration < ?"
      ")";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(now));
  if (!statement.Run())
    return CleanupVisualsTask::Result();

  return CleanupVisualsTask::Result{true, db->GetLastChangeCount()};
}

}  // namespace

CleanupVisualsTask::CleanupVisualsTask(OfflinePageMetadataStore* store,
                                       base::Time now,
                                       CleanupVisualsCallback complete_callback)
    : store_(store),
      now_(now),
      complete_callback_(std::move(complete_callback)) {}

CleanupVisualsTask::~CleanupVisualsTask() = default;

void CleanupVisualsTask::Run() {
  store_->Execute(base::BindOnce(CleanupVisualsSync, now_),
                  base::BindOnce(&CleanupVisualsTask::Complete,
                                 weak_ptr_factory_.GetWeakPtr()),
                  Result());
}

void CleanupVisualsTask::Complete(Result result) {
  TaskComplete();
  UMA_HISTOGRAM_COUNTS_1000("OfflinePages.CleanupThumbnails.Count",
                            result.removed_rows);
  std::move(complete_callback_).Run(result.success);
}

}  // namespace offline_pages
