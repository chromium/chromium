// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/finalize_dismissed_url_suggestion_task.h"

#include <memory>

#include "base/bind.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {

namespace {

bool DeletePageByClientIdIfNotDownloadedSync(const ClientId& client_id,
                                             sql::Database* db) {
  static const std::array<PrefetchItemState, 6>& finalizable_states =
      FinalizeDismissedUrlSuggestionTask::kFinalizableStates;

  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE client_id = ? AND client_namespace = ? "
      " AND state IN (?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(PrefetchItemErrorCode::SUGGESTION_INVALIDATED));
  statement.BindString(2, client_id.id);
  statement.BindString(3, client_id.name_space);
  for (size_t i = 0; i < finalizable_states.size(); ++i)
    statement.BindInt(4 + i, static_cast<int>(finalizable_states[i]));
  return statement.Run();
}
}  // namespace

// static
constexpr std::array<PrefetchItemState, 6>
    FinalizeDismissedUrlSuggestionTask::kFinalizableStates;

FinalizeDismissedUrlSuggestionTask::FinalizeDismissedUrlSuggestionTask(
    PrefetchStore* prefetch_store,
    const ClientId& client_id)
    : prefetch_store_(prefetch_store), client_id_(client_id) {
  DCHECK(prefetch_store_);
}

FinalizeDismissedUrlSuggestionTask::~FinalizeDismissedUrlSuggestionTask() {}

void FinalizeDismissedUrlSuggestionTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&DeletePageByClientIdIfNotDownloadedSync, client_id_),
      base::BindOnce(&FinalizeDismissedUrlSuggestionTask::OnComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void FinalizeDismissedUrlSuggestionTask::OnComplete(bool result) {
  TaskComplete();
}

}  // namespace offline_pages
