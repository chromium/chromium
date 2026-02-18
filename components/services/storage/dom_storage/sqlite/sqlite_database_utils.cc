// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace storage::sqlite {
namespace {

sql::DatabaseOptions GetDatabaseOptions() {
  return sql::DatabaseOptions()
      .set_wal_mode(true)
      // Prevent SQLite from trying to use mmap because
      // `SandboxedVfs` does not support mmap.
      .set_mmap_enabled(false);
}

}  // namespace

StatusOr<
    std::tuple<std::unique_ptr<sql::Database>, std::unique_ptr<sql::MetaTable>>>
OpenDatabase(const base::FilePath& database_path,
             sql::Database::Tag database_tag,
             int current_schema_version,
             int compatible_schema_version,
             CreateSchemaCallback create_schema_callback) {
  std::unique_ptr<sql::Database> database = std::make_unique<sql::Database>(
      GetDatabaseOptions(), std::move(database_tag));

  // Open the database.
  bool is_opened = false;
  const bool is_in_memory = database_path.empty();
  if (is_in_memory) {
    is_opened = database->OpenInMemory();
  } else {
    CHECK(database_path.IsAbsolute());
    is_opened = database->Open(database_path);
  }

  if (!is_opened) {
    return base::unexpected(storage::FromSqliteCode(*database));
  }

  sql::Transaction transaction(database.get());
  if (!transaction.Begin()) {
    return base::unexpected(storage::FromSqliteCode(*database));
  }

  const bool is_new_database = !sql::MetaTable::DoesTableExist(database.get());
  if (is_new_database) {
    // Initialize the brand new database's tables.
    DbStatus status = std::move(create_schema_callback).Run(*database);
    if (!status.ok()) {
      return base::unexpected(std::move(status));
    }
  }

  // Check the database's version.
  std::unique_ptr<sql::MetaTable> meta_table =
      std::make_unique<sql::MetaTable>();
  if (!meta_table->Init(database.get(), current_schema_version,
                        compatible_schema_version)) {
    return base::unexpected(storage::FromSqliteCode(*database));
  }

  if (meta_table->GetCompatibleVersionNumber() > current_schema_version) {
    return base::unexpected(DbStatus::NotFound("Database too new"));
  }

  if (!transaction.Commit()) {
    return base::unexpected(storage::FromSqliteCode(*database));
  }
  return std::make_tuple(std::move(database), std::move(meta_table));
}

DbStatus DestroyDatabase(const base::FilePath& database_path) {
  CHECK(!database_path.empty());
  CHECK(database_path.IsAbsolute());

  if (!sql::Database::Delete(database_path)) {
    return DbStatus::IOError("delete failed");
  }
  return DbStatus::OK();
}

}  // namespace storage::sqlite
