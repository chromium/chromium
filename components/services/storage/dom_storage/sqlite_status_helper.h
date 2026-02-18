// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_STATUS_HELPER_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_STATUS_HELPER_H_

#include "components/services/storage/dom_storage/db_status.h"

namespace sql {
class Database;
}

namespace storage {

// Creates a `DbStatus` using the last `database` operation's error code and
// message.
DbStatus FromSqliteCode(const sql::Database& database);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_STATUS_HELPER_H_
