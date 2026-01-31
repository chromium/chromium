// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"

#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

// Returns a `Status` if the passed expression evaluates to false.
#define RETURN_STATUS_ON_ERROR(expr)            \
  if (!expr) {                                  \
    return storage::FromSqliteCode(*database_); \
  }

namespace storage {
namespace {

constexpr int kCurrentSchemaVersion = 1;
constexpr int kCompatibleSchemaVersion = kCurrentSchemaVersion;

constexpr sql::Database::Tag kLocalStorageTag = "LocalStorage";
constexpr sql::Database::Tag kLocalStorageTagInMemory = "LocalStorageEphemeral";

DbStatus CreateSchema(sql::Database& database) {
  constexpr const char kCreateMapUsageMetadataTable[] =
      // clang-format off
      "CREATE TABLE maps("
        "row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "storage_key BLOB NOT NULL,"
        "last_accessed INTEGER,"
        "last_modified INTEGER,"
        "total_size INTEGER"
      ")";
  // clang-format on

  if (!database.Execute(kCreateMapUsageMetadataTable)) {
    return storage::FromSqliteCode(database);
  }

  constexpr const char kMapsByStorageKeyIndex[] =
      "CREATE UNIQUE INDEX maps_by_storage_key ON maps(storage_key)";

  if (!database.Execute(kMapsByStorageKeyIndex)) {
    return storage::FromSqliteCode(database);
  }
  return MapEntriesTable::CreateSchema(database);
}

}  // namespace

LocalStorageSqlite::LocalStorageSqlite(PassKey) {}

LocalStorageSqlite::~LocalStorageSqlite() = default;

DbStatus LocalStorageSqlite::Open(
    PassKey,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  CHECK(!database_);
  CHECK(!meta_table_);

  ASSIGN_OR_RETURN(
      std::tie(database_, meta_table_),
      sqlite::OpenDatabase(
          database_path,
          database_path.empty() ? kLocalStorageTag : kLocalStorageTagInMemory,
          kCurrentSchemaVersion, kCompatibleSchemaVersion,
          base::BindOnce(&CreateSchema)));

  map_entries_table_ = std::make_unique<MapEntriesTable>(*database_);
  return DbStatus::OK();
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
LocalStorageSqlite::ReadMapKeyValues(MapLocator map_locator) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus LocalStorageSqlite::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus LocalStorageSqlite::CloneMap(MapLocator source_map,
                                      MapLocator target_map) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

StatusOr<DomStorageDatabase::Metadata> LocalStorageSqlite::ReadAllMetadata() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus LocalStorageSqlite::PutMetadata(Metadata metadata) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus LocalStorageSqlite::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus LocalStorageSqlite::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus LocalStorageSqlite::PurgeOrigins(std::set<url::Origin> origins) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus LocalStorageSqlite::RewriteDB() {
  // SQLite does not need to rewrite its database to fully erase deleted data.
  return DbStatus::OK();
}

void LocalStorageSqlite::MakeAllCommitsFailForTesting() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  NOTREACHED();
}

void LocalStorageSqlite::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  NOTREACHED();
}

DbStatus LocalStorageSqlite::PutVersionForTesting(int64_t version) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());
  RETURN_STATUS_ON_ERROR(meta_table_->SetCompatibleVersionNumber(version));
  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

}  // namespace storage
