// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_UTILS_H_

#include <memory>
#include <tuple>

#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/sqlite_status_helper.h"
#include "sql/database.h"

namespace sql {
class MetaTable;
}  // namespace sql

namespace storage::sqlite {
using CreateSchemaCallback = base::OnceCallback<DbStatus(sql::Database&)>;

// Returns a `sql::Database` and `sql::MetaTable` instance for local storage or
// session storage. Creates an in-memory database when `database_path` is
// empty. Runs `create_schema_callback` to initialize the tables for brand new
// database.
StatusOr<
    std::tuple<std::unique_ptr<sql::Database>, std::unique_ptr<sql::MetaTable>>>
OpenDatabase(const base::FilePath& database_path,
             sql::Database::Tag database_tag,
             int current_schema_version,
             int compatible_schema_version,
             CreateSchemaCallback create_schema_callback);

// Deletes the SQLite database file identified by `database_path`.
DbStatus DestroyDatabase(const base::FilePath& database_path);

}  // namespace storage::sqlite

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_UTILS_H_
