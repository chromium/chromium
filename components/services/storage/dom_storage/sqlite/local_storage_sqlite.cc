// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"

#include "base/byte_size.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/sqlite/map_entries_table.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_macros.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

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

std::optional<base::Time> ColumnOptionalTime(sql::Statement& statement,
                                             int column_index) {
  if (statement.GetColumnType(column_index) == sql::ColumnType::kNull) {
    return std::nullopt;
  }
  return statement.ColumnTime(column_index);
}

void BindOptionalTime(sql::Statement& statement,
                      int column_index,
                      std::optional<base::Time> time) {
  if (time) {
    statement.BindTime(column_index, *time);
  } else {
    statement.BindNull(column_index);
  }
}

// Reads an optional `base::ByteSize` from the specified column in `statement`.
// Returns `std::nullopt` if the column contains `NULL`. Returns a corruption
// error if the value is negative.
StatusOr<std::optional<base::ByteSize>> ColumnOptionalByteSize(
    sql::Statement& statement,
    int column_index) {
  if (statement.GetColumnType(column_index) == sql::ColumnType::kNull) {
    return std::nullopt;
  }
  int64_t total_size = statement.ColumnInt64(column_index);
  if (!base::IsValueInRangeForNumericType<uint64_t>(total_size)) {
    return base::unexpected(DbStatus::Corruption("invalid total size"));
  }
  return base::ByteSize(static_cast<uint64_t>(total_size));
}

void BindOptionalByteSize(sql::Statement& statement,
                          int column_index,
                          std::optional<base::ByteSize> byte_size) {
  if (byte_size) {
    statement.BindInt64(column_index,
                        base::checked_cast<int64_t>(byte_size->InBytes()));
  } else {
    statement.BindNull(column_index);
  }
}

}  // namespace

LocalStorageSqlite::LocalStorageSqlite(PassKey) {}

LocalStorageSqlite::~LocalStorageSqlite() {
  if (destruction_callback_for_testing_) {
    std::move(destruction_callback_for_testing_).Run();
  }
}

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
  CHECK_EQ(map_locator.session_ids().size(), 0u);

  sql::Transaction transaction(database_.get());
  RETURN_UNEXPECTED_ON_ERROR(transaction.Begin());

  ASSIGN_OR_RETURN(std::optional<int64_t> map_id,
                   FindMapId(map_locator.storage_key()));
  if (!map_id) {
    // Return an empty map when `map_locator` is not found in the database.
    return {};
  }

  ASSIGN_OR_RETURN((std::map<Key, Value> map_entries),
                   map_entries_table_->GetMapKeyValues(*map_id));

  RETURN_UNEXPECTED_ON_ERROR(transaction.Commit());
  return map_entries;
}

DbStatus LocalStorageSqlite::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  for (MapBatchUpdate& map_update : map_updates) {
    const MapLocator& map_locator = map_update.map_locator;
    CHECK_EQ(map_locator.session_ids().size(), 0u);

    const blink::StorageKey& storage_key = map_locator.storage_key();
    const std::optional<MapBatchUpdate::Usage>& map_usage =
        map_update.map_usage;

    std::optional<int64_t> map_id;

    // If requested, delete the map's metadata.
    if (map_usage && map_usage->should_delete_all_usage()) {
      // A map with key/value pairs must provide usage metadata.
      CHECK(map_update.entries_to_add.empty());

      // Look up the `map_id` (`row_id`) for this storage key.  The `map_id`
      // might not exist when clearing a map that is already empty.
      ASSIGN_OR_RETURN(map_id, FindMapId(storage_key));
      if (!map_id) {
        continue;
      }

      DB_RETURN_IF_ERROR(DeleteMapMetadata({storage_key}));
    } else {
      // Insert or update the map's metadata.  This creates the map row if it
      // doesn't exist, assigning a `map_id` (`row_id`) to new maps.
      // `LocalStorageSqlite` expects a new map's first update to create its
      // `map_id` through the function call below.
      DB_RETURN_IF_ERROR(PutMapMetadata(
          storage_key, map_usage ? map_usage->last_accessed() : std::nullopt,
          map_usage ? map_usage->last_modified() : std::nullopt,
          map_usage ? map_usage->total_size() : std::nullopt));

      // Find the `map_id`, which must exist since `PutMapMetadata()` creates it
      // above when necessary.
      ASSIGN_OR_RETURN(map_id, FindMapId(storage_key));
    }

    // Update `map_locator` with the assigned `map_id` so that
    // `MapEntriesTable::UpdateMap()` can use `map_id` to write key/value pairs.
    map_update.map_locator = MapLocator(storage_key, map_id.value());

    // Apply the key/value pair changes (additions, modifications, deletions)
    // to the `map_entries` table.
    DB_RETURN_IF_ERROR(map_entries_table_->UpdateMap(std::move(map_update)));
  }

  if (should_fail_commits_for_testing_) {
    return DbStatus::IOError("Simulated I/O Error");
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus LocalStorageSqlite::CloneMap(MapLocator source_map,
                                      MapLocator target_map) {
  // Local storage does not support cloning.
  NOTREACHED();
}

StatusOr<DomStorageDatabase::Metadata> LocalStorageSqlite::ReadAllMetadata() {
  constexpr const char kSelectAllMaps[] =
      "SELECT storage_key, row_id, last_accessed, last_modified, total_size "
      "FROM maps";

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kSelectAllMaps));

  std::vector<MapMetadata> map_metadata;
  while (statement.Step()) {
    ASSIGN_OR_RETURN(
        blink::StorageKey storage_key,
        blink::StorageKey::Deserialize(statement.ColumnBlobAsString(0)),
        []() { return DbStatus::Corruption("invalid storage key"); });

    ASSIGN_OR_RETURN(std::optional<base::ByteSize> total_size,
                     ColumnOptionalByteSize(statement, 4));

    map_metadata.push_back({
        .map_locator{
            std::move(storage_key),
            /*map_id=*/statement.ColumnInt64(1),
        },
        .last_accessed = ColumnOptionalTime(statement, 2),
        .last_modified = ColumnOptionalTime(statement, 3),
        .total_size = total_size,
    });
  }

  RETURN_UNEXPECTED_ON_ERROR(statement.Succeeded());
  return Metadata(std::move(map_metadata));
}

DbStatus LocalStorageSqlite::PutMetadata(Metadata metadata) {
  // Local storage does not record the next map id in SQLite.
  CHECK(!metadata.next_map_id);

  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  for (const MapMetadata& map_metadata : metadata.map_metadata) {
    DB_RETURN_IF_ERROR(PutMapMetadata(
        map_metadata.map_locator.storage_key(), map_metadata.last_accessed,
        map_metadata.last_modified, map_metadata.total_size));
  }

  if (should_fail_commits_for_testing_) {
    return DbStatus::IOError("Simulated I/O Error");
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus LocalStorageSqlite::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  // Local storage uses a single global session without clones.  To avoid
  // orphaned maps, each deleted storage key must also delete its map.
  CHECK_EQ(session_id, std::string());
  CHECK_EQ(maps_to_delete.size(), metadata_to_delete.size());

  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  DB_RETURN_IF_ERROR(DeleteMapMetadata(metadata_to_delete));

  // Delete the key/value pairs in `maps_to_delete`.
  for (const MapLocator& map : maps_to_delete) {
    // A valid `map` must be in `metadata_to_delete`.
    CHECK_EQ(map.session_ids().size(), 0u);
    DCHECK(std::ranges::contains(metadata_to_delete, map.storage_key()));

    ASSIGN_OR_RETURN(std::optional<int64_t> map_id,
                     FindMapId(map.storage_key()));
    if (map_id) {
      DB_RETURN_IF_ERROR(map_entries_table_->DeleteMap(*map_id));
    }
  }

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

DbStatus LocalStorageSqlite::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  // Potential callers should delete the entire database instead of the session.
  // Local storage uses a single global session.
  NOTREACHED();
}

DbStatus LocalStorageSqlite::PurgeOrigins(std::set<url::Origin> origins) {
  return ::storage::PurgeOrigins(*this, std::move(origins));
}

DbStatus LocalStorageSqlite::RewriteDB() {
  // SQLite does not need to rewrite its database to fully erase deleted data.
  return DbStatus::OK();
}

void LocalStorageSqlite::MakeAllCommitsFailForTesting() {
  should_fail_commits_for_testing_ = true;
}

void LocalStorageSqlite::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  destruction_callback_for_testing_ = std::move(callback);
}

DbStatus LocalStorageSqlite::PutVersionForTesting(int64_t version) {
  sql::Transaction transaction(database_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());
  RETURN_STATUS_ON_ERROR(meta_table_->SetCompatibleVersionNumber(version));
  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return DbStatus::OK();
}

StatusOr<std::optional<int64_t>> LocalStorageSqlite::FindMapId(
    const blink::StorageKey& storage_key) {
  constexpr const char kSelectMapId[] =
      "SELECT row_id FROM maps WHERE storage_key = ?";

  sql::Statement statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kSelectMapId));
  statement.BindBlob(0, storage_key.Serialize());

  if (!statement.Step()) {
    RETURN_UNEXPECTED_ON_ERROR(statement.Succeeded());

    // No map found for this storage key.  Return an empty map.
    return std::nullopt;
  }
  return /*map_id=*/statement.ColumnInt64(0);
}

DbStatus LocalStorageSqlite::PutMapMetadata(
    const blink::StorageKey& storage_key,
    std::optional<base::Time> last_accessed,
    std::optional<base::Time> last_modified,
    std::optional<base::ByteSize> total_size) {
  CHECK(database_->HasActiveTransactions());
  std::string serialized_storage_key = storage_key.Serialize();

  // First, try to update an existing row. Use `COALESCE` to preserve existing
  // values when the new value is `NULL`, allowing partial updates.
  constexpr const char kUpdateMap[] =
      // clang-format off
      "UPDATE maps "
      "SET "
        "last_accessed = COALESCE(?, last_accessed),"
        "last_modified = COALESCE(?, last_modified),"
        "total_size = COALESCE(?, total_size) "
      "WHERE storage_key = ?";
  // clang-format on

  sql::Statement update_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kUpdateMap));

  BindOptionalTime(update_statement, 0, last_accessed);
  BindOptionalTime(update_statement, 1, last_modified);
  BindOptionalByteSize(update_statement, 2, total_size);
  update_statement.BindBlob(3, serialized_storage_key);

  RETURN_STATUS_ON_ERROR(update_statement.Run());

  // If no row was updated, insert a new row.
  if (database_->GetLastChangeCount() == 0) {
    constexpr const char kInsertMap[] =
        "INSERT INTO maps"
        "(storage_key, last_accessed, last_modified, total_size) "
        "VALUES(?, ?, ?, ?)";

    sql::Statement insert_statement(
        database_->GetCachedStatement(SQL_FROM_HERE, kInsertMap));

    insert_statement.BindBlob(0, serialized_storage_key);
    BindOptionalTime(insert_statement, 1, last_accessed);
    BindOptionalTime(insert_statement, 2, last_modified);
    BindOptionalByteSize(insert_statement, 3, total_size);

    RETURN_STATUS_ON_ERROR(insert_statement.Run());
  }
  return DbStatus::OK();
}

DbStatus LocalStorageSqlite::DeleteMapMetadata(
    const std::vector<blink::StorageKey>& metadata_to_delete) {
  CHECK(database_->HasActiveTransactions());

  // Delete each storage key's metadata from the `maps` table.
  constexpr const char kDeleteMapMetadata[] =
      "DELETE FROM maps WHERE storage_key = ?";

  sql::Statement delete_statement(
      database_->GetCachedStatement(SQL_FROM_HERE, kDeleteMapMetadata));

  for (const blink::StorageKey& storage_key : metadata_to_delete) {
    delete_statement.BindBlob(0, storage_key.Serialize());
    RETURN_STATUS_ON_ERROR(delete_statement.Run());
    delete_statement.Reset(/*clear_bound_vars=*/true);
  }

  RETURN_STATUS_ON_ERROR(delete_statement.Run());
  return DbStatus::OK();
}

}  // namespace storage
