// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/sent_get_operation_cleanup_task.h"

#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

std::unique_ptr<std::vector<std::string>> GetAllOperationsSync(
    sql::Database* db) {
  static const char kSql[] =
      "SELECT DISTINCT operation_name"
      " FROM prefetch_items"
      " WHERE state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::SENT_GET_OPERATION));

  std::unique_ptr<std::vector<std::string>> operation_names;
  while (statement.Step()) {
    if (!operation_names)
      operation_names = std::make_unique<std::vector<std::string>>();
    operation_names->push_back(statement.ColumnString(0));
  }
  return operation_names;
}

bool UpdateOperationSync(const std::string& operation_name,
                         int max_attempts,
                         sql::Database* db) {
  // For all items in SENT_GET_OPERATION state and matching |operation_name|:
  // * transit to RECEIVED_GCM state if not exceeding maximum attempts
  // * transit to FINISHED state with error_code set otherwise.
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = CASE WHEN get_operation_attempts < ? THEN ?"
      "                  ELSE ? END,"
      "     error_code = CASE WHEN get_operation_attempts < ? THEN error_code"
      "                       ELSE ? END"
      " WHERE state = ? AND operation_name = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, max_attempts);
  statement.BindInt(1, static_cast<int>(PrefetchItemState::RECEIVED_GCM));
  statement.BindInt(2, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(3, max_attempts);
  statement.BindInt(
      4, static_cast<int>(
             PrefetchItemErrorCode::GET_OPERATION_MAX_ATTEMPTS_REACHED));
  statement.BindInt(5, static_cast<int>(PrefetchItemState::SENT_GET_OPERATION));
  statement.BindString(6, operation_name);

  return statement.Run();
}

bool CleanupOperationsSync(
    std::unique_ptr<std::set<std::string>> ongoing_operation_names,
    int max_attempts,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  std::unique_ptr<std::vector<std::string>> tracked_operation_names =
      GetAllOperationsSync(db);
  if (!tracked_operation_names)
    return false;

  for (const auto& tracked_operation_name : *tracked_operation_names) {
    // If the operation request is still running, do nothing.
    if (ongoing_operation_names &&
        ongoing_operation_names->count(tracked_operation_name) > 0)
      continue;

    // Otherwise, update the state to either retry the opeation or error out.
    if (!UpdateOperationSync(tracked_operation_name, max_attempts, db))
      return false;
  }

  return transaction.Commit();
}

}  // namespace

// static
const int SentGetOperationCleanupTask::kMaxGetOperationAttempts = 3;

SentGetOperationCleanupTask::SentGetOperationCleanupTask(
    PrefetchStore* prefetch_store,
    PrefetchNetworkRequestFactory* request_factory)
    : prefetch_store_(prefetch_store), request_factory_(request_factory) {}

SentGetOperationCleanupTask::~SentGetOperationCleanupTask() {}

void SentGetOperationCleanupTask::Run() {
  std::unique_ptr<std::set<std::string>> ongoing_operation_names =
      request_factory_->GetAllOperationNamesRequested();

  prefetch_store_->Execute(
      base::BindOnce(&CleanupOperationsSync, std::move(ongoing_operation_names),
                     kMaxGetOperationAttempts),
      base::BindOnce(&SentGetOperationCleanupTask::OnFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void SentGetOperationCleanupTask::OnFinished(bool success) {
  TaskComplete();
}

}  // namespace offline_pages
