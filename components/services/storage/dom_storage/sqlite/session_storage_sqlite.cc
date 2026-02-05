// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"

#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_macros.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace storage {
namespace {

constexpr int kCurrentSchemaVersion = 1;
constexpr int kCompatibleSchemaVersion = kCurrentSchemaVersion;

constexpr sql::Database::Tag kSessionStorageTag = "SessionStorage";
constexpr sql::Database::Tag kSessionStorageTagInMemory =
    "SessionStorageEphemeral";

inline constexpr const char kNextMapIdKey[] = "next_map_id";

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

// Parses a single row from the `session_metadata` table into a `MapMetadata`
// struct. The statement must have columns in order: `session_id` (TEXT),
// `storage_key` (BLOB), `map_id` (INTEGER). Returns an error if the
// `storage_key` cannot be deserialized.
StatusOr<DomStorageDatabase::MapMetadata> ParseMapMetadata(
    sql::Statement& statement) {
  ASSIGN_OR_RETURN(
      blink::StorageKey storage_key,
      blink::StorageKey::Deserialize(statement.ColumnBlobAsString(1)),
      []() { return DbStatus::Corruption("invalid storage key"); });

  return DomStorageDatabase::MapMetadata{
      .map_locator{
          /*session_id=*/statement.ColumnString(0),
          std::move(storage_key),
          /*map_id=*/statement.ColumnInt(2),
      },
  };
}

}  // namespace

SessionStorageSqlite::SessionStorageSqlite(PassKey) {}

SessionStorageSqlite::~SessionStorageSqlite() {
  if (destruction_callback_for_testing_) {
    std::move(destruction_callback_for_testing_).Run();
  }
}

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
  int64_t map_id = map_locator.map_id().value();
  return map_entries_table_->GetMapKeyValues(map_id);
}

DbStatus SessionStorageSqlite::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  for (MapBatchUpdate& map_update : map_updates) {
    // Session storage does not record map usage metadata.
    CHECK(!map_update.map_usage.has_value());

    DB_RETURN_IF_ERROR(map_entries_table_->UpdateMap(std::move(map_update)));
  }

  if (should_fail_commits_for_testing_) {
    return DbStatus::IOError("Simulated I/O Error");
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus SessionStorageSqlite::CloneMap(MapLocator source_map,
                                        MapLocator target_map) {
  // Copy all key/value pairs from the source map to the target map.
  return map_entries_table_->CloneMap(source_map.map_id().value(),
                                      target_map.map_id().value());
}

StatusOr<DomStorageDatabase::Metadata> SessionStorageSqlite::ReadAllMetadata() {
  sql::Transaction transaction(database_.get());
  RETURN_UNEXPECTED_ON_ERROR(transaction.Begin());

  Metadata all_metadata;
  all_metadata.next_map_id = ReadNextMapId();
  ASSIGN_OR_RETURN(all_metadata.map_metadata, ReadAllMapMetadata());

  RETURN_UNEXPECTED_ON_ERROR(transaction.Commit());
  return all_metadata;
}

DbStatus SessionStorageSqlite::PutMetadata(Metadata metadata) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  // Update the `next_map_id` in the meta table if provided.
  if (metadata.next_map_id) {
    RETURN_STATUS_ON_ERROR(
        meta_table_->SetValue(kNextMapIdKey, *metadata.next_map_id));
  }

  const char kPutMapMetadata[] =
      "INSERT OR REPLACE INTO session_metadata "
      "(storage_key, map_id, session_id) VALUES (?, ?, ?)";

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kPutMapMetadata));

  // Insert or replace rows in the `session_metadata` table for each map.
  for (const MapMetadata& map_metadata : metadata.map_metadata) {
    const MapLocator& map_locator = map_metadata.map_locator;

    std::string serialized_storage_key = map_locator.storage_key().Serialize();
    statement.BindBlob(0, std::move(serialized_storage_key));

    int64_t map_id = map_locator.map_id().value();
    statement.BindInt64(1, map_id);

    // Write one row per session. For cloned maps, this results in multiple
    // rows with the same (storage_key, map_id) but different session_ids.
    for (const std::string& session_id : map_locator.session_ids()) {
      statement.BindString(2, session_id);
      RETURN_STATUS_ON_ERROR(statement.Run());
      statement.Reset(/*clear_bound_vars=*/false);
    }
  }

  if (should_fail_commits_for_testing_) {
    return DbStatus::IOError("Simulated I/O Error");
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus SessionStorageSqlite::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  // Delete each storage key's metadata from the session.
  constexpr const char kDeleteSessionMetadata[] =
      "DELETE FROM session_metadata WHERE session_id = ? AND storage_key = ?";

  sql::Statement delete_metadata_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteSessionMetadata));

  for (const blink::StorageKey& storage_key : metadata_to_delete) {
    delete_metadata_statement.BindString(0, session_id);
    delete_metadata_statement.BindBlob(1, storage_key.Serialize());
    RETURN_STATUS_ON_ERROR(delete_metadata_statement.Run());
    delete_metadata_statement.Reset(/*clear_bound_vars=*/true);
  }

  // Delete the key/value pairs in `maps_to_delete`.
  for (const MapLocator& map_locator : maps_to_delete) {
    // The map's storage key must be in `metadata_to_delete`.
    DCHECK(
        std::ranges::contains(metadata_to_delete, map_locator.storage_key()));

    // The map must be unreferenced with no sessions remaining.
    CHECK(map_locator.session_ids().empty());

    DB_RETURN_IF_ERROR(
        map_entries_table_->DeleteMap(map_locator.map_id().value()));
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus SessionStorageSqlite::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  // Delete each session's metadata.
  constexpr const char kDeleteSessionMetadata[] =
      "DELETE FROM session_metadata WHERE session_id = ?";

  sql::Statement delete_metadata_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteSessionMetadata));

  for (const std::string& session_id : session_ids) {
    delete_metadata_statement.BindString(0, session_id);
    RETURN_STATUS_ON_ERROR(delete_metadata_statement.Run());
    delete_metadata_statement.Reset(/*clear_bound_vars=*/true);
  }

  // Delete the key/value pairs in `maps_to_delete`.
  for (const MapLocator& map_locator : maps_to_delete) {
    // The map must be unreferenced with no sessions remaining.
    CHECK(map_locator.session_ids().empty());

    DB_RETURN_IF_ERROR(
        map_entries_table_->DeleteMap(map_locator.map_id().value()));
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus SessionStorageSqlite::PurgeOrigins(std::set<url::Origin> origins) {
  // Origins aren't explicitly purged from session storage on shutdown because
  // all session storage (generally) is cleared on shutdown already.
  NOTREACHED();
}

DbStatus SessionStorageSqlite::RewriteDB() {
  // SQLite does not need to rewrite its database to fully erase deleted data.
  return DbStatus::OK();
}

void SessionStorageSqlite::MakeAllCommitsFailForTesting() {
  should_fail_commits_for_testing_ = true;
}

void SessionStorageSqlite::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  destruction_callback_for_testing_ = std::move(callback);
}

DbStatus SessionStorageSqlite::PutVersionForTesting(int64_t version) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());
  RETURN_STATUS_ON_ERROR(meta_table_->SetCompatibleVersionNumber(version));
  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

int64_t SessionStorageSqlite::ReadNextMapId() const {
  int64_t next_map_id;
  if (!meta_table_->GetValue(kNextMapIdKey, &next_map_id)) {
    return 0;
  }
  return next_map_id;
}

StatusOr<std::vector<DomStorageDatabase::MapMetadata>>
SessionStorageSqlite::ReadAllMapMetadata() const {
  // Associates a `map_id` to its merged `MapMetadata`. Used to detect and merge
  // cloned maps that share the same `map_id` across different sessions.
  absl::flat_hash_map</*map_id=*/int64_t, MapMetadata> all_metadata;

  const char kSelectAllMetadata[] =
      "SELECT session_id, storage_key, map_id FROM session_metadata";

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kSelectAllMetadata));

  while (statement.Step()) {
    ASSIGN_OR_RETURN(MapMetadata map_metadata, ParseMapMetadata(statement));

    const DomStorageDatabase::MapLocator& map_locator =
        map_metadata.map_locator;

    // Each row in the database represents exactly one session's reference to
    // a map, so the parsed `MapMetadata` must have exactly one session ID.
    CHECK_EQ(map_locator.session_ids().size(), 1u);
    const std::string& session_id = map_locator.session_ids()[0];
    int64_t map_id = map_locator.map_id().value();

    auto [iter, inserted] =
        all_metadata.try_emplace(map_id, std::move(map_metadata));
    if (!inserted) {
      // This `map_id` was already used by another session, meaning it's a
      // cloned map. Add this session to the existing `MapMetadata` session
      // list.
      iter->second.map_locator.AddSession(session_id);
    }
  }

  RETURN_UNEXPECTED_ON_ERROR(statement.Succeeded());

  // Convert the hash map to a vector for the return value.
  std::vector<DomStorageDatabase::MapMetadata> results;
  results.reserve(all_metadata.size());

  for (auto& [map_id, metadata] : all_metadata) {
    results.push_back(std::move(metadata));
  }
  return results;
}

}  // namespace storage
