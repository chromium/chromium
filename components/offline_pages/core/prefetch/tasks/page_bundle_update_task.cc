// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/page_bundle_update_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
using PageBundleUpdateResult = PageBundleUpdateTask::PageBundleUpdateResult;

namespace {

// Marks a successfully rendered URL as having received the bundle, and returns
// whether any records matched the given RenderPageInfo.
bool MarkUrlRenderedSync(sql::Database* db,
                         const RenderPageInfo& page,
                         const std::string& operation_name) {
  DCHECK_EQ(page.status, RenderStatus::RENDERED);

  // This method may be called upon receiving the results of GeneratePageBundle
  // or GetOperation. For GeneratePageBundle, the operation name is not yet set
  // in the database. For GetOperation, the operation name is already set.
  // This statement ensures that the item's operation_name is assigned, and that
  // an item can't be reassigned an operation name.
  static const char kSql[] = R"(UPDATE prefetch_items
    SET state = ?,
        final_archived_url = ?,
        archive_body_name = ?,
        archive_body_length = ?,
        operation_name = ?
    WHERE requested_url = ? AND state IN (?, ?) AND operation_name IN ("", ?)
  )";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  DCHECK(statement.is_valid());

  std::string final_url = page.redirect_url;
  if (final_url == page.url)
    final_url = "";

  // SET
  statement.BindInt(0, static_cast<int>(PrefetchItemState::RECEIVED_BUNDLE));
  statement.BindString(1, final_url);
  statement.BindString(2, page.body_name);
  statement.BindInt64(3, page.body_length);
  statement.BindString(4, operation_name);

  // WHERE
  statement.BindString(5, page.url);
  statement.BindInt(
      6, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));
  statement.BindInt(7, static_cast<int>(PrefetchItemState::SENT_GET_OPERATION));
  statement.BindString(8, operation_name);

  if (!statement.Run())
    return false;

  return db->GetLastChangeCount() > 0;
}

// Marks a URL that failed to render as finished.
void MarkUrlFailedSync(sql::Database* db,
                       const RenderPageInfo& page,
                       const std::string& operation_name,
                       PrefetchItemErrorCode final_status) {
  DCHECK_NE(page.status, RenderStatus::RENDERED);

  static const char kSql[] = R"(UPDATE prefetch_items
    SET state = ?,
        error_code = ?,
        operation_name = ?
    WHERE requested_url = ? AND state IN (?, ?) AND operation_name IN ("", ?)
  )";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  DCHECK(statement.is_valid());

  std::string final_url = page.redirect_url;
  if (final_url.empty())
    final_url = page.url;

  // SET
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(final_status));
  statement.BindString(2, operation_name);

  // WHERE
  statement.BindString(3, page.url);
  statement.BindInt(
      4, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));
  statement.BindInt(5, static_cast<int>(PrefetchItemState::SENT_GET_OPERATION));
  statement.BindString(6, operation_name);

  statement.Run();
}

// Marks URLs known to be PENDING as awaiting GCM.
void MarkAwaitingGCMSync(sql::Database* db,
                         const RenderPageInfo& page,
                         const std::string& operation_name) {
  static const char kSql[] = R"(UPDATE prefetch_items
    SET state = ?,
        operation_name = ?
    WHERE state = ? AND requested_url = ?
  )";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  DCHECK(statement.is_valid());

  // SET
  statement.BindInt(0, static_cast<int>(PrefetchItemState::AWAITING_GCM));
  statement.BindString(1, operation_name);

  // WHERE
  statement.BindInt(
      2, static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE));
  statement.BindString(3, page.url);

  statement.Run();
}

// Individually updates all pages for the given operation
PageBundleUpdateResult UpdateWithOperationResultsSync(
    const std::string operation_name,
    const std::vector<RenderPageInfo>& pages,
    sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  bool schedule_pipeline_processing = false;

  for (auto& page : pages) {
    switch (page.status) {
      case RenderStatus::RENDERED:
        if (MarkUrlRenderedSync(db, page, operation_name))
          schedule_pipeline_processing = true;
        break;
      case RenderStatus::FAILED:
        MarkUrlFailedSync(db, page, operation_name,
                          PrefetchItemErrorCode::ARCHIVING_FAILED);
        break;
      case RenderStatus::EXCEEDED_LIMIT:
        MarkUrlFailedSync(db, page, operation_name,
                          PrefetchItemErrorCode::ARCHIVING_LIMIT_EXCEEDED);
        break;
      case RenderStatus::PENDING:
        MarkAwaitingGCMSync(db, page, operation_name);
        break;
    }
  }

  return transaction.Commit() && schedule_pipeline_processing;
}

}  // namespace

PageBundleUpdateTask::PageBundleUpdateTask(
    PrefetchStore* store,
    PrefetchDispatcher* dispatcher,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages)
    : store_(store),
      dispatcher_(dispatcher),
      operation_name_(operation_name),
      pages_(pages) {
  DCHECK(store_);
}

PageBundleUpdateTask::~PageBundleUpdateTask() = default;

void PageBundleUpdateTask::Run() {
  if (pages_.empty()) {
    FinishedWork(false);
    return;
  }

  store_->Execute(
      base::BindOnce(&UpdateWithOperationResultsSync, operation_name_, pages_),
      base::BindOnce(&PageBundleUpdateTask::FinishedWork,
                     weak_factory_.GetWeakPtr()),
      false);
}

void PageBundleUpdateTask::FinishedWork(
    PageBundleUpdateResult needs_pipeline_processing) {
  if (needs_pipeline_processing)
    dispatcher_->SchedulePipelineProcessing();
  TaskComplete();
}

}  // namespace offline_pages
