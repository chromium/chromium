// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"

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

constexpr sql::Database::Tag kSessionStorageTag = "SessionStorage";
constexpr sql::Database::Tag kSessionStorageTagInMemory =
    "SessionStorageEphemeral";

DbStatus CreateSchema(sql::Database& database) {
  constexpr const char kCreateSessionMetadataTable[] =
      // clang-format off
      "CREATE TABLE session_metadata("
        "session_id TEXT NOT NULL,"
        "storage_key BLOB NOT NULL,"
        "map_id INTEGER NOT NULL,"
        "PRIMARY KEY(session_id, storage_key)"
      ") WITHOUT ROWID";
  // clang-format on

  if (!database.Execute(kCreateSessionMetadataTable)) {
    return storage::FromSqliteCode(database);
  }
  return MapEntriesTable::CreateSchema(database);
}

}  // namespace

SessionStorageSqlite::SessionStorageSqlite(PassKey) {}

SessionStorageSqlite::~SessionStorageSqlite() = default;

DbStatus SessionStorageSqlite::Open(
    PassKey,
    const base::FilePath& database_path,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  CHECK(!database_);
  CHECK(!meta_table_);

  ASSIGN_OR_RETURN(
      std::tie(database_, meta_table_),
      sqlite::OpenDatabase(database_path,
                           database_path.empty() ? kSessionStorageTag
                                                 : kSessionStorageTagInMemory,
                           kCurrentSchemaVersion, kCompatibleSchemaVersion,
                           base::BindOnce(&CreateSchema)));

  map_entries_table_ = std::make_unique<MapEntriesTable>(*database_);
  return DbStatus::OK();
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
SessionStorageSqlite::ReadMapKeyValues(MapLocator map_locator) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus SessionStorageSqlite::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageSqlite::CloneMap(MapLocator source_map,
                                        MapLocator target_map) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

StatusOr<DomStorageDatabase::Metadata> SessionStorageSqlite::ReadAllMetadata() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return base::unexpected(DbStatus::NotSupported(""));
}

DbStatus SessionStorageSqlite::PutMetadata(Metadata metadata) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageSqlite::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageSqlite::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageSqlite::PurgeOrigins(std::set<url::Origin> origins) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageSqlite::RewriteDB() {
  // SQLite does not need to rewrite its database to fully erase deleted data.
  return DbStatus::OK();
}

void SessionStorageSqlite::MakeAllCommitsFailForTesting() {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  NOTREACHED();
}

void SessionStorageSqlite::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  // TODO(crbug.com/377242771): Fully implement `DomStorageDatabase` interface
  // using SQLite.
  NOTREACHED();
}

DbStatus SessionStorageSqlite::PutVersionForTesting(int64_t version) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());
  RETURN_STATUS_ON_ERROR(meta_table_->SetCompatibleVersionNumber(version));
  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

}  // namespace storage
