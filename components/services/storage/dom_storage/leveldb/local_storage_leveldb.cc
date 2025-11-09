// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"

#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_database.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace storage {

// Returns a copy of `source` as a byte vector.
std::vector<uint8_t> ToBytes(std::string source) {
  return std::vector<uint8_t>(source.begin(), source.end());
}

// Returns "<prefix><serialized `storage_key`>".
DomStorageDatabase::Key CreatePrefixedStorageKey(
    const DomStorageDatabase::KeyView& prefix,
    const blink::StorageKey& storage_key) {
  const std::string serialized_storage_key =
      storage_key.SerializeForLocalStorage();

  base::span<const uint8_t> serialized_storage_key_bytes =
      base::as_byte_span(serialized_storage_key);

  DomStorageDatabase::Key result;
  result.reserve(prefix.size() + serialized_storage_key.size());

  // Append `prefix`.
  result.insert(result.end(), prefix.begin(), prefix.end());

  // Append `storage_key`.
  result.insert(result.end(), serialized_storage_key_bytes.begin(),
                serialized_storage_key_bytes.end());
  return result;
}

// Removes a prefix like "META:" or "METAACCESS:" from `key` and then attempts
// to deserialize a `StorageKey` using the resulting substring.
std::optional<blink::StorageKey> TryExtractStorageKeyFromPrefixedKey(
    const DomStorageDatabase::Key& key,
    const DomStorageDatabase::KeyView& prefix) {
  if (key.size() < prefix.size()) {
    return std::nullopt;
  }
  return blink::StorageKey::DeserializeForLocalStorage(
      base::as_string_view(key).substr(prefix.size()));
}

// Parses a `StorageKey` using the key from `leveldb_meta_entry`. Parses a
// `LocalStorageAreaWriteMetaData` protobuf using the value from
// `leveldb_meta_entry`. Returns std::nullopt when parsing fails.
std::optional<DomStorageDatabase::MapMetadata> TryParseWriteMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_entry) {
  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(leveldb_meta_entry.key,
                                          kWriteMetaPrefix);
  if (!storage_key) {
    return std::nullopt;
  }

  storage::LocalStorageAreaWriteMetaData write_metadata;
  if (!write_metadata.ParseFromArray(leveldb_meta_entry.value.data(),
                                     leveldb_meta_entry.value.size())) {
    return std::nullopt;
  }

  return DomStorageDatabase::MapMetadata{
      .map_locator{
          kLocalStorageSessionId,
          *std::move(storage_key),
      },
      .last_modified{
          base::Time::FromInternalValue(write_metadata.last_modified())},
      .total_size{write_metadata.size_bytes()},
  };
}

// Same as `TryParseWriteMetadata()` above, but parses a
// `LocalStorageAreaAccessMetaData` protobuf using the value from
// `leveldb_meta_entry`.
std::optional<DomStorageDatabase::MapMetadata> TryParseAccessMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_access_entry) {
  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(leveldb_meta_access_entry.key,
                                          kAccessMetaPrefix);
  if (!storage_key) {
    return std::nullopt;
  }

  storage::LocalStorageAreaAccessMetaData access_metadata;
  if (!access_metadata.ParseFromArray(leveldb_meta_access_entry.value.data(),
                                      leveldb_meta_access_entry.value.size())) {
    return std::nullopt;
  }

  return DomStorageDatabase::MapMetadata{
      .map_locator{
          kLocalStorageSessionId,
          *std::move(storage_key),
      },
      .last_accessed{
          base::Time::FromInternalValue(access_metadata.last_accessed())},
  };
}

LocalStorageLevelDB::LocalStorageLevelDB(PassKey) {}

LocalStorageLevelDB::~LocalStorageLevelDB() = default;

DbStatus LocalStorageLevelDB::Open(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  ASSIGN_OR_RETURN(leveldb_, DomStorageDatabaseLevelDB::Open(directory, name,
                                                             memory_dump_id));
  return DbStatus::OK();
}

DomStorageDatabase::Key LocalStorageLevelDB::CreateAccessMetaDataKey(
    const blink::StorageKey& storage_key) {
  return CreatePrefixedStorageKey(kAccessMetaPrefix, storage_key);
}

DomStorageDatabase::Key LocalStorageLevelDB::CreateWriteMetaDataKey(
    const blink::StorageKey& storage_key) {
  return CreatePrefixedStorageKey(kWriteMetaPrefix, storage_key);
}

DomStorageDatabase::Value LocalStorageLevelDB::CreateAccessMetaDataValue(
    base::Time last_accessed) {
  storage::LocalStorageAreaAccessMetaData metadata;
  metadata.set_last_accessed(last_accessed.ToInternalValue());
  return ToBytes(metadata.SerializeAsString());
}

DomStorageDatabase::Value LocalStorageLevelDB::CreateWriteMetaDataValue(
    base::Time last_modified,
    base::ByteSize total_size) {
  storage::LocalStorageAreaWriteMetaData metadata;
  metadata.set_last_modified(last_modified.ToInternalValue());
  metadata.set_size_bytes(total_size.InBytes());
  return ToBytes(metadata.SerializeAsString());
}

DomStorageDatabaseLevelDB& LocalStorageLevelDB::GetLevelDB() {
  return *leveldb_;
}

StatusOr<DomStorageDatabase::Metadata> LocalStorageLevelDB::ReadAllMetadata() {
  std::vector<DomStorageDatabase::KeyValuePair> leveldb_metadata_entries;
  DbStatus status =
      leveldb_->GetPrefixed(kMetaPrefix, &leveldb_metadata_entries);
  if (!status.ok()) {
    return base::unexpected(std::move(status));
  }

  // Each map may have up to two LevelDB metadata entries for a single
  // `StorageKey`: one for "META:" and one for "METAACCESS:". Collapse both of
  // the map's entries into a single `MapMetadata`.
  absl::flat_hash_map<blink::StorageKey, DomStorageDatabase::MapMetadata>
      storage_key_metadata_map;

  for (const DomStorageDatabase::KeyValuePair& leveldb_metadata_entry :
       leveldb_metadata_entries) {
    std::optional<DomStorageDatabase::MapMetadata> parsed_metadata =
        TryParseWriteMetadata(leveldb_metadata_entry);

    if (!parsed_metadata) {
      parsed_metadata = TryParseAccessMetadata(leveldb_metadata_entry);
    }

    if (!parsed_metadata) {
      // ERROR: Invalid metadata! Ignore database corruption by skipping to
      // the next entry.
      continue;
    }

    // Add `parsed_metadata` to `storage_key_metadata_map`, combining "META:"
    // and "METAACCESS:" for the same storage key.
    const blink::StorageKey& storage_key =
        parsed_metadata->map_locator.storage_key();

    auto it = storage_key_metadata_map.find(storage_key);
    if (it == storage_key_metadata_map.end()) {
      // Add a new `MapMetadata`.
      storage_key_metadata_map.emplace(storage_key,
                                       *std::move(parsed_metadata));
    } else {
      // Update the existing storage key's metadata to also include this
      // entry's. Each storage key must have at most one "META:" entry and
      // one "METAACCESS:" entry, which must not overwrite any existing
      // values.
      DomStorageDatabase::MapMetadata& existing_metadata = it->second;
      if (parsed_metadata->last_accessed) {
        CHECK(!existing_metadata.last_accessed);
        existing_metadata.last_accessed = parsed_metadata->last_accessed;
      }
      if (parsed_metadata->last_modified) {
        CHECK(!existing_metadata.last_modified);
        existing_metadata.last_modified = parsed_metadata->last_modified;
      }
      if (parsed_metadata->total_size) {
        CHECK(!existing_metadata.total_size);
        existing_metadata.total_size = parsed_metadata->total_size;
      }
    }
  }

  // Create a vector of `MapMetadata` to return using the map's values.
  std::vector<DomStorageDatabase::MapMetadata> results;
  for (std::pair<const blink::StorageKey, DomStorageDatabase::MapMetadata>&
           storage_key_metadata : storage_key_metadata_map) {
    results.emplace_back(std::move(storage_key_metadata.second));
  }
  return Metadata{std::move(results)};
}

DbStatus LocalStorageLevelDB::PutMetadata(Metadata metadata) {
  // Local storage does not record the next map id in LevelDB.
  CHECK(!metadata.next_map_id);

  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  // Record usage for each map in `metadata`.
  for (const DomStorageDatabase::MapMetadata& map_usage :
       metadata.map_metadata) {
    const blink::StorageKey& storage_key = map_usage.map_locator.storage_key();

    if (map_usage.last_accessed) {
      // Add "METAACCESS:" entry.
      batch->Put(CreateAccessMetaDataKey(storage_key),
                 CreateAccessMetaDataValue(*map_usage.last_accessed));
    }

    if (map_usage.last_modified && map_usage.total_size) {
      // Add "META:" entry.
      batch->Put(CreateWriteMetaDataKey(storage_key),
                 CreateWriteMetaDataValue(*map_usage.last_modified,
                                          *map_usage.total_size));
    }
  }
  return batch->Commit();
}

DbStatus LocalStorageLevelDB::RewriteDB() {
  return leveldb_->RewriteDB();
}

bool LocalStorageLevelDB::ShouldFailAllCommits() {
  return leveldb_->ShouldFailAllCommits();
}

void LocalStorageLevelDB::MakeAllCommitsFailForTesting() {
  leveldb_->MakeAllCommitsFailForTesting();
}

void LocalStorageLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  leveldb_->SetDestructionCallbackForTesting(std::move(callback));
}

}  // namespace storage
