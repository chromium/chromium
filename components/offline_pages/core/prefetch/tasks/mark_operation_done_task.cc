// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/mark_operation_done_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

bool UpdatePrefetchItemsSync(sql::Database* db,
                             const std::string& operation_name) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, freshness_time = ?"
      " WHERE state = ? AND operation_name = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::RECEIVED_GCM));
  statement.BindInt64(1, store_utils::ToDatabaseTime(OfflineTimeNow()));
  statement.BindInt(2, static_cast<int>(PrefetchItemState::AWAITING_GCM));
  statement.BindString(3, operation_name);

  return statement.Run();
}

MarkOperationDoneTask::TaskResult MakeStoreError() {
  return std::make_pair(MarkOperationDoneTask::StoreResult::STORE_ERROR, -1);
}

// Will hold the actual SQL implementation for marking a MarkOperationDone
// attempt in the database.
MarkOperationDoneTask::TaskResult MarkOperationCompletedOnServerSync(
    const std::string& operation_name,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (transaction.Begin() && UpdatePrefetchItemsSync(db, operation_name) &&
      transaction.Commit()) {
    return std::make_pair(MarkOperationDoneTask::StoreResult::UPDATED,
                          db->GetLastChangeCount());
  }
  return MakeStoreError();
}

}  // namespace

MarkOperationDoneTask::MarkOperationDoneTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const std::string& operation_name)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      operation_name_(operation_name) {}

MarkOperationDoneTask::~MarkOperationDoneTask() {}

void MarkOperationDoneTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&MarkOperationCompletedOnServerSync, operation_name_),
      base::BindOnce(&MarkOperationDoneTask::Done, weak_factory_.GetWeakPtr()),
      MakeStoreError());
}

void MarkOperationDoneTask::Done(TaskResult result) {
  result_ = result;

  // We need to make sure we can process any work that was created by this event
  // so we will ensure the task is scheudled.
  if (change_count() > 0)
    prefetch_dispatcher_->EnsureTaskScheduled();

  TaskComplete();
}

}  // namespace offline_pages
