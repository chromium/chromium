// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/remove_url_task.h"

#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {

namespace {

bool RemoveURL(const GURL& url, sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state=?,error_code=?"
      " WHERE requested_url=?"
      " AND state NOT IN (?,?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(PrefetchItemErrorCode::SUGGESTION_INVALIDATED));
  statement.BindString(2, url.spec());
  statement.BindInt(3, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(4, static_cast<int>(PrefetchItemState::ZOMBIE));
  return statement.Run();
}

}  // namespace

std::unique_ptr<SqlCallbackTask> MakeRemoveUrlTask(PrefetchStore* store,
                                                   const GURL& url) {
  return std::make_unique<SqlCallbackTask>(store,
                                           base::BindOnce(&RemoveURL, url));
}

}  // namespace offline_pages
