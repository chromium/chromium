// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"

#include <algorithm>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb_utils.h"
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
  return CreatePrefixedKey(prefix, base::as_byte_span(serialized_storage_key));
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
          *std::move(storage_key),
      },
      .last_accessed{
          base::Time::FromInternalValue(access_metadata.last_accessed())},
  };
}

// Returns "METAACCESS:<serialized `storage_key`>".
DomStorageDatabase::Key CreateAccessMetaDataKey(
    const blink::StorageKey& storage_key) {
  return CreatePrefixedStorageKey(kAccessMetaPrefix, storage_key);
}

// Returns "META:<serialized `storage_key`>".
DomStorageDatabase::Key CreateWriteMetaDataKey(
    const blink::StorageKey& storage_key) {
  return CreatePrefixedStorageKey(kWriteMetaPrefix, storage_key);
}

// Return the the serialized bytes for the `LocalStorageAreaAccessMetaData`
// protobuf with `last_accessed`.
DomStorageDatabase::Value CreateAccessMetaDataValue(base::Time last_accessed) {
  storage::LocalStorageAreaAccessMetaData metadata;
  metadata.set_last_accessed(last_accessed.ToInternalValue());
  return ToBytes(metadata.SerializeAsString());
}

// Return the the serialized bytes for the `LocalStorageAreaWriteMetaData`
// protobuf with `last_modified` and `total_size`.
DomStorageDatabase::Value CreateWriteMetaDataValue(base::Time last_modified,
                                                   base::ByteSize total_size) {
  storage::LocalStorageAreaWriteMetaData metadata;
  metadata.set_last_modified(last_modified.ToInternalValue());
  metadata.set_size_bytes(total_size.InBytes());
  return ToBytes(metadata.SerializeAsString());
}

// Returns "_<storage key>\x00", which matches all of the map key/value pairs
// for `storage_key`.
DomStorageDatabase::Key GetMapPrefix(const blink::StorageKey& storage_key) {
  const std::string serialized_storage_key =
      storage_key.SerializeForLocalStorage();

  constexpr char kMapPrefixStart[] = {'_'};

  DomStorageDatabase::Key map_prefix;
  map_prefix.reserve(std::size(kMapPrefixStart) +
                     serialized_storage_key.size() +
                     /*kLocalStorageKeyMapSeparator=*/1);

  // Append '_'.
  map_prefix.push_back(kMapPrefixStart[0]);

  // Append `storage_key`.
  map_prefix.insert(map_prefix.end(), serialized_storage_key.begin(),
                    serialized_storage_key.end());

  // Append a null byte: '0x00'.
  map_prefix.push_back(kLocalStorageKeyMapSeparator);
  return map_prefix;
}

LocalStorageLevelDB::LocalStorageLevelDB(PassKey) {}

LocalStorageLevelDB::~LocalStorageLevelDB() = default;

DbStatus LocalStorageLevelDB::Open(
    PassKey,
    const base::FilePath& directory,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id) {
  ASSIGN_OR_RETURN(leveldb_,
                   DomStorageDatabaseLevelDB::Open(
                       StorageType::kLocalStorage, directory, memory_dump_id,
                       kLocalStorageLevelDBVersionKey,
                       /*min_supported_version=*/kLocalStorageLevelDBVersion,
                       /*max_supported_version=*/kLocalStorageLevelDBVersion));
  return DbStatus::OK();
}

StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
LocalStorageLevelDB::ReadMapKeyValues(MapLocator map_locator) {
  CHECK_EQ(map_locator.session_ids().size(), 0u);
  return leveldb_->GetMapKeyValues(GetMapPrefix(map_locator.storage_key()));
}

DbStatus LocalStorageLevelDB::UpdateMaps(
    std::vector<MapBatchUpdate> map_updates) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> leveldb_batch =
      leveldb_->CreateBatchOperation();

  for (const MapBatchUpdate& map_update : map_updates) {
    const MapLocator& map_locator = map_update.map_locator;
    CHECK_EQ(map_locator.session_ids().size(), 0u);

    DomStorageDatabase::Key map_prefix =
        GetMapPrefix(map_locator.storage_key());

    DB_RETURN_IF_ERROR(
        leveldb_batch->UpdateMapKeyValues(map_prefix, map_update));

    // Optionally update the map's usage metadata.
    if (!map_update.map_usage) {
      continue;
    }

    if (map_update.map_usage->should_delete_all_usage()) {
      DeleteMapUsageMetadata(*leveldb_batch, map_locator.storage_key());
    } else {
      PutMapUsageMetadata(*leveldb_batch, map_locator.storage_key(),
                          map_update.map_usage->last_accessed(),
                          map_update.map_usage->last_modified(),
                          map_update.map_usage->total_size());
    }
  }
  return leveldb_batch->Commit();
}

DbStatus LocalStorageLevelDB::CloneMap(MapLocator source_map,
                                       MapLocator target_map) {
  // Local storage does not support cloning.
  NOTREACHED();
}

StatusOr<DomStorageDatabase::Metadata> LocalStorageLevelDB::ReadAllMetadata() {
  ASSIGN_OR_RETURN(
      std::vector<DomStorageDatabase::KeyValuePair> leveldb_metadata_entries,
      leveldb_->GetPrefixed(kMetaPrefix));

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
  results.reserve(storage_key_metadata_map.size());

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
    PutMapUsageMetadata(*batch, map_usage.map_locator.storage_key(),
                        map_usage.last_accessed, map_usage.last_modified,
                        map_usage.total_size);
  }
  return batch->Commit();
}

DbStatus LocalStorageLevelDB::DeleteStorageKeysFromSession(
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<MapLocator> maps_to_delete) {
  // Local storage uses a single global session without clones.  To avoid
  // orphaned maps, each deleted storage key must also delete its map.
  CHECK_EQ(session_id, std::string());
  CHECK_EQ(maps_to_delete.size(), metadata_to_delete.size());

  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      leveldb_->CreateBatchOperation();

  for (const blink::StorageKey& storage_key : metadata_to_delete) {
    DeleteMapUsageMetadata(*batch, storage_key);
  }

  // Erase all map key/value pairs.
  for (const MapLocator& map : maps_to_delete) {
    // A valid `map` must be in `storage_keys`.
    CHECK_EQ(map.session_ids().size(), 0u);
    DCHECK(std::ranges::contains(metadata_to_delete, map.storage_key()));

    DB_RETURN_IF_ERROR(batch->DeletePrefixed(GetMapPrefix(map.storage_key())));
  }
  return batch->Commit();
}

DbStatus LocalStorageLevelDB::DeleteSessions(
    std::vector<std::string> session_ids,
    std::vector<MapLocator> maps_to_delete) {
  // Not implemented.  Since local storage uses a single global session, callers
  // should delete the entire database instead of the session.
  NOTREACHED();
}

DbStatus LocalStorageLevelDB::PurgeOrigins(std::set<url::Origin> origins) {
  return ::storage::PurgeOrigins(*this, std::move(origins));
}

DbStatus LocalStorageLevelDB::RewriteDB() {
  return leveldb_->RewriteDB();
}

DbStatus LocalStorageLevelDB::PutVersionForTesting(int64_t version) {
  return leveldb_->Put(kLocalStorageLevelDBVersionKey,
                       base::as_byte_span(base::NumberToString(version)));
}

void LocalStorageLevelDB::MakeAllCommitsFailForTesting() {
  leveldb_->MakeAllCommitsFailForTesting();
}

void LocalStorageLevelDB::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  leveldb_->SetDestructionCallbackForTesting(std::move(callback));
}

DomStorageDatabaseLevelDB& LocalStorageLevelDB::GetLevelDBForTesting() {
  return *leveldb_;
}

void LocalStorageLevelDB::PutMapUsageMetadata(
    DomStorageBatchOperationLevelDB& batch,
    const blink::StorageKey& map_storage_key,
    std::optional<base::Time> last_accessed,
    std::optional<base::Time> last_modified,
    std::optional<base::ByteSize> total_size) {
  // `PutMapUsageMetadata()` must have at least one value to write.
  CHECK(last_accessed || last_modified);

  // The "META:" entry requires both `last_modified` and `total_size`.
  CHECK_EQ(last_modified.has_value(), total_size.has_value());

  if (last_accessed) {
    // Add "METAACCESS:" entry.
    batch.Put(CreateAccessMetaDataKey(map_storage_key),
              CreateAccessMetaDataValue(*last_accessed));
  }

  if (last_modified) {
    // Add "META:" entry.
    batch.Put(CreateWriteMetaDataKey(map_storage_key),
              CreateWriteMetaDataValue(*last_modified, *total_size));
  }
}

void LocalStorageLevelDB::DeleteMapUsageMetadata(
    DomStorageBatchOperationLevelDB& batch,
    const blink::StorageKey& map_storage_key) {
  // Erase the "METAACCESS:" entry.
  batch.Delete(CreateAccessMetaDataKey(map_storage_key));

  // Erase the "META:" entry.
  batch.Delete(CreateWriteMetaDataKey(map_storage_key));
}

}  // namespace storage
