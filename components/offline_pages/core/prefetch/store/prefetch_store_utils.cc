// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"

#include <limits>

#include "base/rand_util.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {

// static
bool PrefetchStoreUtils::DeletePrefetchItemByOfflineIdSync(sql::Database* db,
                                                           int64_t offline_id) {
  DCHECK(db);
  static const char kSql[] = "DELETE FROM prefetch_items WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

}  // namespace offline_pages
