// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
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

SessionStorageLevelDB::SessionStorageLevelDB(PassKey) {}

SessionStorageLevelDB::~SessionStorageLevelDB() = default;

DbStatus SessionStorageLevelDB::Open(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  ASSIGN_OR_RETURN(
      leveldb_,
      DomStorageDatabaseLevelDB::Open(
          directory, name, memory_dump_id, kSessionStorageLevelDBVersionKey,
          /*min_supported_version=*/kSessionStorageLevelDBVersion,
          /*max_supported_version=*/kSessionStorageLevelDBVersion));
  return DbStatus::OK();
}

DomStorageDatabaseLevelDB& SessionStorageLevelDB::GetLevelDB() {
  return *leveldb_;
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

    Key key = CreateMapMetadataKey(map_locator.session_id(),
                                   map_locator.storage_key());

    std::string value = base::NumberToString(map_locator.map_id().value());
    batch->Put(std::move(key), base::as_byte_span(value));
  }
  return batch->Commit();
}

DbStatus SessionStorageLevelDB::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> storage_keys,
    absl::flat_hash_set<int64_t> excluded_cloned_map_ids) {
  // TODO(crbug.com/377242771): Implement and use for session storage.
  return DbStatus::NotSupported("");
}

DbStatus SessionStorageLevelDB::RewriteDB() {
  return leveldb_->RewriteDB();
}

void SessionStorageLevelDB::MakeAllCommitsFailForTesting() {
  leveldb_->MakeAllCommitsFailForTesting();
}

void SessionStorageLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  leveldb_->SetDestructionCallbackForTesting(std::move(callback));
}

DomStorageDatabase::Key SessionStorageLevelDB::CreateMapMetadataKey(
    std::string session_id,
    const blink::StorageKey& storage_key) {
  // `session_id` must be a GUID string.
  CHECK_EQ(session_id.size(), blink::kSessionStorageNamespaceIdLength);

  std::string serialized_storage_key = storage_key.Serialize();

  Key key;
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

  // Create a `MapMetadata` for each entry.
  std::vector<DomStorageDatabase::MapMetadata> results;
  for (const KeyValuePair& namespace_entry : namespace_entries) {
    ASSIGN_OR_RETURN(MapMetadata map_metadata,
                     ParseMapMetadata(namespace_entry));
    results.push_back(std::move(map_metadata));
  }
  return results;
}

}  // namespace storage
