// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"

namespace storage {

StatusOr<DomStorageDatabase::MapMetadata> ParseMapMetadata(
    const DomStorageDatabase::KeyValuePair& namespace_entry) {
  constexpr const char kInvalidKeyError[] = "namespace key is too short";

  // Expected key format: 'namespace-<session id guid>-<storage key>'.
  // For example:
  // 'namespace-2b437ef2_a816_4f5f_b4fd_0e2e4da516a8-https://example.test/'
  std::string_view key = base::as_string_view(namespace_entry.key);

  // The key must start with 'namespace-'.
  CHECK(base::StartsWith(key, base::as_string_view(kNamespacePrefix)));

  // Remove 'namespace-'.
  key = key.substr(std::size(kNamespacePrefix));

  // The key must include a session id guid.
  if (key.size() < blink::kSessionStorageNamespaceIdLength) {
    return base::unexpected(DbStatus::Corruption(kInvalidKeyError));
  }
  std::string session_id =
      std::string(key.substr(0, blink::kSessionStorageNamespaceIdLength));

  // '-' must separate the session id and storage key.
  key = key.substr(blink::kSessionStorageNamespaceIdLength);
  if (key.empty() || key[0] != kNamespaceStorageKeySeparator) {
    return base::unexpected(DbStatus::Corruption(kInvalidKeyError));
  }
  key = key.substr(1);

  // Only the storage key remains.
  std::optional<blink::StorageKey> storage_key =
      blink::StorageKey::Deserialize(key);
  if (!storage_key) {
    return base::unexpected(
        DbStatus::Corruption("namespace storage key is invalid"));
  }

  // Parse the map ID integer text string.
  int64_t map_id;
  if (!base::StringToInt64(base::as_string_view(namespace_entry.value),
                           &map_id)) {
    return base::unexpected(
        DbStatus::Corruption("namespace map id is not a number"));
  }

  return DomStorageDatabase::MapMetadata{
      .map_locator{
          std::move(session_id),
          *std::move(storage_key),
          map_id,
      },
  };
}

// Returns "namespace-<session_id>-"
DomStorageDatabase::Key GetSessionPrefix(const std::string& session_id) {
  DomStorageDatabase::Key session_prefix;
  session_prefix.reserve(std::size(kNamespacePrefix) + session_id.size() +
                         /*kNamespaceStorageKeySeparator=*/1);

  // Append "namespace-"
  session_prefix.insert(session_prefix.end(), std::begin(kNamespacePrefix),
                        std::end(kNamespacePrefix));

  // Append `session_id`.
  session_prefix.insert(session_prefix.end(), session_id.begin(),
                        session_id.end());

  // Append "-".
  session_prefix.push_back(kNamespaceStorageKeySeparator);
  return session_prefix;
}

// Returns `namespace-<session_id>-<storage_key>`.
DomStorageDatabase::Key CreateMapMetadataKey(
    std::string session_id,
    const blink::StorageKey& storage_key) {
  // `session_id` must be a GUID string.
  CHECK_EQ(session_id.size(), blink::kSessionStorageNamespaceIdLength);

  std::string serialized_storage_key = storage_key.Serialize();

  DomStorageDatabase::Key key;
  key.reserve(std::size(kNamespacePrefix) + session_id.size() +
              /*kNamespaceStorageKeySeparator=*/1 +
              serialized_storage_key.length());

  // Add 'namespace-'.
  key.insert(key.end(), std::begin(kNamespacePrefix),
             std::end(kNamespacePrefix));

  // Append `session_id`.
  key.insert(key.end(), session_id.begin(), session_id.end());

  // Append '-'.
  key.push_back(kNamespaceStorageKeySeparator);

  // Append `storage_key`.
  key.insert(key.end(), serialized_storage_key.begin(),
             serialized_storage_key.end());
  return key;
}

// Returns the prefix for all key/value pairs that belong to a specific map.
// For example: "map-1-".
DomStorageDatabase::Key GetMapPrefix(int64_t map_id) {
  std::string map_id_text = base::NumberToString(map_id);

  DomStorageDatabase::Key map_prefix;
  map_prefix.reserve(std::size(kMapIdPrefix) + map_id_text.size() +
                     /*kMapIdKeySeparator=*/1);

  // Append "map-".
  map_prefix.insert(map_prefix.end(), std::begin(kMapIdPrefix),
                    std::end(kMapIdPrefix));

  // Append `map_id` as text.
  map_prefix.insert(map_prefix.end(), map_id_text.begin(), map_id_text.end());

  // Append "-".
  map_prefix.push_back(kMapIdKeySeparator);
  return map_prefix;
}

SessionStorageLevelDB::SessionStorageLevelDB(PassKey) {}

SessionStorageLevelDB::~SessionStorageLevelDB() = default;

DbStatus SessionStorageLevelDB::Open(
    PassKey,
    const base::FilePath& directory,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  ASSIGN_OR_RETURN(
      leveldb_, DomStorageDatabaseLevelDB::Open(
                    StorageType::kSessionStorage, directory, memory_dump_id,
                    kSessionStorageLevelDBVersionKey,
                    /*min_supported_version=*/kSessionStorageLevelDBVersion,
                    /*max_supported_version=*/kSessionStorageLevelDBVersion));
  return DbStatus::OK();
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
SessionStorageLevelDB::ReadMapKeyValues(MapLocator map_locator) {
  return leveldb_->GetMapKeyValues(GetMapPrefix(map_locator.map_id().value()));
}

DbStatus SessionStorageLevelDB::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> leveldb_batch =
      leveldb_->CreateBatchOperation();

  for (const MapBatchUpdate& map_update : map_updates) {
    // Session storage must not record map usage metadata.
    CHECK(!map_update.map_usage.has_value());

    const MapLocator& map_locator = map_update.map_locator;
    DomStorageDatabase::Key map_prefix =
        GetMapPrefix(map_locator.map_id().value());

    DB_RETURN_IF_ERROR(
        leveldb_batch->UpdateMapKeyValues(map_prefix, map_update));
  }
  return leveldb_batch->Commit();
}

DbStatus SessionStorageLevelDB::CloneMap(MapLocator source_map,
                                         MapLocator target_map) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  // Copy the key/value pairs from `source_map` to `target_map`.
  DB_RETURN_IF_ERROR(
      batch->CopyPrefixed(GetMapPrefix(source_map.map_id().value()),
                          GetMapPrefix(target_map.map_id().value())));
  return batch->Commit();
}

StatusOr<DomStorageDatabase::Metadata>
SessionStorageLevelDB::ReadAllMetadata() {
  Metadata metadata;
  ASSIGN_OR_RETURN(metadata.next_map_id, ReadNextMapId());
  ASSIGN_OR_RETURN(metadata.map_metadata, ReadAllMapMetadata());
  return metadata;
}

DbStatus SessionStorageLevelDB::PutMetadata(Metadata metadata) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  // Write the next map ID when provided.
  if (metadata.next_map_id) {
    batch->Put(kNextMapIdKey,
               base::as_byte_span(base::NumberToString(*metadata.next_map_id)));
  }

  // Write the metadata for each map in `metadata.map_metadata`
  for (const MapMetadata& map_metadata : metadata.map_metadata) {
    const MapLocator& map_locator = map_metadata.map_locator;

    std::string map_id_value =
        base::NumberToString(map_locator.map_id().value());

    // Write metadata for each session's map usage.  Multiple cloned sessions
    // use the same map, but create separate metadata entries in the database.
    for (const std::string& session_id : map_locator.session_ids()) {
      Key key = CreateMapMetadataKey(session_id, map_locator.storage_key());
      batch->Put(std::move(key), base::as_byte_span(map_id_value));
    }
  }
  return batch->Commit();
}

DbStatus SessionStorageLevelDB::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  // Delete each storage key's metadata.
  for (const blink::StorageKey& storage_key : metadata_to_delete) {
    batch->Delete(CreateMapMetadataKey(session_id, storage_key));
  }

  // Delete the key/value pairs in `maps_to_delete`.
  for (const DomStorageDatabase::MapLocator& map : maps_to_delete) {
    // A valid `map` must be in `storage_keys` and `session_id`.
    CHECK(map.session_ids().empty());
    DCHECK(std::ranges::contains(metadata_to_delete, map.storage_key()));

    DB_RETURN_IF_ERROR(
        batch->DeletePrefixed(GetMapPrefix(map.map_id().value())));
  }
  return batch->Commit();
}

DbStatus SessionStorageLevelDB::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  // Delete each session's metadata.
  for (const std::string& session_id : session_ids) {
    DB_RETURN_IF_ERROR(batch->DeletePrefixed(GetSessionPrefix(session_id)));
  }

  // Delete the key/value pairs in `maps_to_delete`.
  for (const DomStorageDatabase::MapLocator& map : maps_to_delete) {
    CHECK(map.session_ids().empty());

    DB_RETURN_IF_ERROR(
        batch->DeletePrefixed(GetMapPrefix(map.map_id().value())));
  }
  return batch->Commit();
}

DbStatus SessionStorageLevelDB::PurgeOrigins(std::set<url::Origin> origins) {
  // Origins aren't explicitly purged from session storage on shutdown because
  // all session storage (generally) is cleared on shutdown already.
  NOTREACHED();
}

DbStatus SessionStorageLevelDB::RewriteDB() {
  return leveldb_->RewriteDB();
}

DbStatus SessionStorageLevelDB::PutVersionForTesting(int64_t version) {
  return leveldb_->Put(kSessionStorageLevelDBVersionKey,
                       base::as_byte_span(base::NumberToString(version)));
}

void SessionStorageLevelDB::MakeAllCommitsFailForTesting() {
  leveldb_->MakeAllCommitsFailForTesting();
}

void SessionStorageLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  leveldb_->SetDestructionCallbackForTesting(std::move(callback));
}

DomStorageDatabaseLevelDB& SessionStorageLevelDB::GetLevelDBForTesting() {
  return *leveldb_;
}

StatusOr<int64_t> SessionStorageLevelDB::ReadNextMapId() const {
  StatusOr<Value> map_id_bytes = leveldb_->Get(kNextMapIdKey);
  if (!map_id_bytes.has_value()) {
    if (map_id_bytes.error().IsNotFound()) {
      // Empty databases start with zero for the next map ID.
      return 0;
    }

    // Failed to read the LevelDB.
    return base::unexpected(std::move(map_id_bytes).error());
  }

  // Convert the integer text string to an `int64_t`.
  int64_t next_map_id;
  if (!base::StringToInt64(base::as_string_view(*map_id_bytes), &next_map_id)) {
    return base::unexpected(
        DbStatus::Corruption("next map id is not a number"));
  }
  return next_map_id;
}

StatusOr<std::vector<DomStorageDatabase::MapMetadata>>
SessionStorageLevelDB::ReadAllMapMetadata() const {
  // Read all 'namespace-' prefixed entries from the LevelDB.
  ASSIGN_OR_RETURN(std::vector<KeyValuePair> namespace_entries,
                   leveldb_->GetPrefixed(kNamespacePrefix));

  // Create a `MapMetadata` for each map by parsing `namespace_entries` from
  // LevelDB.
  //
  // Maintain a `flat_hash_map` of map IDs to `MapMetadata` to detect cloned
  // maps. Maps cloned across sessions use the same map ID.
  absl::flat_hash_map</*map_id=*/int64_t, MapMetadata> all_metadata;
  for (const KeyValuePair& namespace_entry : namespace_entries) {
    ASSIGN_OR_RETURN(MapMetadata map_metadata,
                     ParseMapMetadata(namespace_entry));

    const DomStorageDatabase::MapLocator& map_locator =
        map_metadata.map_locator;

    // The LevelDB metadata contains one entry for each map in the session.
    CHECK_EQ(map_locator.session_ids().size(), 1u);
    const std::string& session_id = map_locator.session_ids()[0];
    int64_t map_id = map_locator.map_id().value();

    // Is `map_id` a clone from another session?
    auto it = all_metadata.find(map_id);
    if (it == all_metadata.end()) {
      // This is a new unique map. Create a new `MapMetadata`.
      all_metadata.emplace(std::make_pair(map_id, std::move(map_metadata)));
    } else {
      // This is a clone. Add the session to the existing `MapMetadata`.
      it->second.map_locator.AddSession(session_id);
    }
  }

  std::vector<DomStorageDatabase::MapMetadata> results;
  results.reserve(all_metadata.size());

  for (auto& [map_id, metadata] : all_metadata) {
    results.push_back(std::move(metadata));
  }
  return results;
}

}  // namespace storage
