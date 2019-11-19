// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/get_visuals_info_task.h"

#include <utility>

#include "base/bind.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {
namespace {

GetVisualsInfoTask::Result GetVisualsInfoSync(int64_t offline_id,
                                              sql::Database* db) {
  static const char kSql[] =
      "SELECT thumbnail_url,favicon_url FROM prefetch_items WHERE offline_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  DCHECK(statement.is_valid());

  GetVisualsInfoTask::Result result;
  statement.BindInt64(0, offline_id);
  if (statement.Step()) {
    result.thumbnail_url = GURL(statement.ColumnString(0));
    result.favicon_url = GURL(statement.ColumnString(1));
  }

  return result;
}

}  // namespace

GetVisualsInfoTask::GetVisualsInfoTask(PrefetchStore* store,
                                       int64_t offline_id,
                                       ResultCallback callback)
    : prefetch_store_(store),
      offline_id_(offline_id),
      callback_(std::move(callback)) {}

GetVisualsInfoTask::~GetVisualsInfoTask() = default;

void GetVisualsInfoTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(GetVisualsInfoSync, offline_id_),
      base::BindOnce(&GetVisualsInfoTask::CompleteTaskAndForwardResult,
                     weak_factory_.GetWeakPtr()),
      Result());
}

void GetVisualsInfoTask::CompleteTaskAndForwardResult(Result result) {
  TaskComplete();
  std::move(callback_).Run(result);
}

}  // namespace offline_pages
