// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_metadata.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"
#include "url/gurl.h"

namespace storage {

// For a description of the session storage LevelDB schema, see comments in
// `leveldb/session_storage_leveldb.h

namespace {

// This is "map-" (without the quotes).
constexpr const uint8_t kMapIdPrefixBytes[] = {'m', 'a', 'p', '-'};

std::vector<uint8_t> NumberToValue(int64_t map_number) {
  auto str = base::NumberToString(map_number);
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

SessionStorageMetadata::MapData::MapData(int64_t map_number,
                                         blink::StorageKey storage_key)
    : number_as_bytes_(NumberToValue(map_number)),
      map_id_(map_number),
      key_prefix_(SessionStorageMetadata::GetMapPrefix(number_as_bytes_)),
      storage_key_(std::move(storage_key)) {}
SessionStorageMetadata::MapData::~MapData() = default;

DomStorageDatabase::Metadata SessionStorageMetadata::ToDomStorageMetadata(
    NamespaceEntry session) {
  const std::map<blink::StorageKey,
                 scoped_refptr<SessionStorageMetadata::MapData>>& session_maps =
      session->second;

  DomStorageDatabase::Metadata metadata;
  for (const auto& [storage_key, map_data] : session_maps) {
    metadata.map_metadata.push_back({
        .map_locator{
            /*session_id=*/session->first,
            storage_key,
            map_data->map_id(),
        },
    });
  }
  return metadata;
}

SessionStorageMetadata::SessionStorageMetadata() = default;

SessionStorageMetadata::~SessionStorageMetadata() = default;

std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>
SessionStorageMetadata::SetupNewDatabaseForTesting() {
  next_map_id_ = 0;
  namespace_storage_key_map_.clear();

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  tasks.push_back(base::BindOnce(
      [](int64_t next_map_id, DomStorageBatchOperationLevelDB& batch,
         const DomStorageDatabaseLevelDB& db) {
        batch.Put(base::span(kNextMapIdKey), NumberToValue(next_map_id));
      },
      next_map_id_));
  return tasks;
}

void SessionStorageMetadata::Initialize(DomStorageDatabase::Metadata source) {
  namespace_storage_key_map_.clear();
  next_map_id_ = source.next_map_id.value();

  // Since the data is ordered, all namespace data is in one spot. This keeps a
  // reference to the last namespace data map to be more efficient.
  std::string last_namespace_id;
  std::map<blink::StorageKey, scoped_refptr<MapData>>* last_namespace = nullptr;
  std::map<int64_t, scoped_refptr<MapData>> maps;

  for (const DomStorageDatabase::MapMetadata& source_map :
       source.map_metadata) {
    const std::string& namespace_id = source_map.map_locator.session_id();
    const blink::StorageKey& storage_key = source_map.map_locator.storage_key();
    int64_t map_number = source_map.map_locator.map_id().value();

    if (map_number >= next_map_id_) {
      next_map_id_ = map_number + 1;
    }

    if (namespace_id != last_namespace_id) {
      last_namespace_id = namespace_id;
      DCHECK(namespace_storage_key_map_.find(last_namespace_id) ==
             namespace_storage_key_map_.end());
      last_namespace = &(namespace_storage_key_map_[last_namespace_id]);
    }
    auto map_it = maps.find(map_number);
    if (map_it == maps.end()) {
      map_it =
          maps.emplace(
                  std::piecewise_construct, std::forward_as_tuple(map_number),
                  std::forward_as_tuple(new MapData(map_number, storage_key)))
              .first;
    }
    map_it->second->IncReferenceCount();

    last_namespace->emplace(std::make_pair(storage_key, map_it->second));
  }
}

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageMetadata::RegisterNewMap(NamespaceEntry namespace_entry,
                                       const blink::StorageKey& storage_key) {
  auto new_map_data = base::MakeRefCounted<MapData>(next_map_id_, storage_key);
  ++next_map_id_;

  std::map<blink::StorageKey, scoped_refptr<MapData>>& namespace_storage_keys =
      namespace_entry->second;
  auto namespace_it = namespace_storage_keys.find(storage_key);
  if (namespace_it != namespace_storage_keys.end()) {
    // Check the old map doesn't have the same number as the new map.
    DCHECK(namespace_it->second->MapNumberAsBytes() !=
           new_map_data->MapNumberAsBytes());
    DCHECK_GT(namespace_it->second->ReferenceCount(), 1)
        << "A new map should never be registered for an area that has a "
           "single-refcount map.";
    // There was already an area key here, so decrement that map reference.
    namespace_it->second->DecReferenceCount();
    namespace_it->second = new_map_data;
  } else {
    namespace_storage_keys.emplace(std::make_pair(storage_key, new_map_data));
  }
  new_map_data->IncReferenceCount();
  return new_map_data;
}

void SessionStorageMetadata::RegisterShallowClonedNamespace(
    NamespaceEntry source_namespace,
    NamespaceEntry destination_namespace) {
  std::map<blink::StorageKey, scoped_refptr<MapData>>& source_storage_keys =
      source_namespace->second;
  std::map<blink::StorageKey, scoped_refptr<MapData>>&
      destination_storage_keys = destination_namespace->second;
  DCHECK_EQ(0ul, destination_storage_keys.size())
      << "The destination already has data.";

  for (const auto& storage_key_map_pair : source_storage_keys) {
    destination_storage_keys.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(storage_key_map_pair.first),
        std::forward_as_tuple(storage_key_map_pair.second));
    storage_key_map_pair.second->IncReferenceCount();
  }
}

void SessionStorageMetadata::DeleteNamespace(
    const std::string& namespace_id,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks) {
  std::vector<DomStorageDatabase::Key> prefixes_to_delete;
  auto it = namespace_storage_key_map_.find(namespace_id);
  if (it == namespace_storage_key_map_.end())
    return;

  prefixes_to_delete.push_back(GetNamespacePrefix(namespace_id));

  const std::map<blink::StorageKey, scoped_refptr<MapData>>& storage_keys =
      it->second;
  for (const auto& storage_key_map_pair : storage_keys) {
    MapData* map_data = storage_key_map_pair.second.get();
    DCHECK_GT(map_data->ReferenceCount(), 0);
    map_data->DecReferenceCount();
    if (map_data->ReferenceCount() == 0)
      prefixes_to_delete.push_back(map_data->KeyPrefix());
  }

  namespace_storage_key_map_.erase(it);

  save_tasks->push_back(base::BindOnce(
      [](std::vector<DomStorageDatabase::Key> prefixes_to_delete,
         DomStorageBatchOperationLevelDB& batch,
         const DomStorageDatabaseLevelDB& db) {
        for (const auto& prefix : prefixes_to_delete)
          std::ignore = batch.DeletePrefixed(prefix);
      },
      std::move(prefixes_to_delete)));
}

void SessionStorageMetadata::DeleteArea(
    const std::string& namespace_id,
    const blink::StorageKey& storage_key,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks) {
  auto ns_entry = namespace_storage_key_map_.find(namespace_id);
  if (ns_entry == namespace_storage_key_map_.end())
    return;

  auto storage_key_map_it = ns_entry->second.find(storage_key);
  if (storage_key_map_it == ns_entry->second.end())
    return;

  MapData* map_data = storage_key_map_it->second.get();

  DomStorageDatabase::Key area_key =
      SessionStorageLevelDB::CreateMapMetadataKey(namespace_id, storage_key);
  std::vector<DomStorageDatabase::Key> prefixes_to_delete;
  DCHECK_GT(map_data->ReferenceCount(), 0);
  map_data->DecReferenceCount();
  if (map_data->ReferenceCount() == 0)
    prefixes_to_delete.push_back(map_data->KeyPrefix());

  ns_entry->second.erase(storage_key_map_it);

  save_tasks->push_back(base::BindOnce(
      [](const DomStorageDatabase::Key& area_key,
         std::vector<DomStorageDatabase::Key> prefixes_to_delete,
         DomStorageBatchOperationLevelDB& batch,
         const DomStorageDatabaseLevelDB& db) {
        batch.Delete(area_key);
        for (const auto& prefix : prefixes_to_delete)
          std::ignore = batch.DeletePrefixed(prefix);
      },
      area_key, std::move(prefixes_to_delete)));
}

SessionStorageMetadata::NamespaceEntry
SessionStorageMetadata::GetOrCreateNamespaceEntry(
    const std::string& namespace_id) {
  // Note: if the entry exists, emplace will return the existing entry and NOT
  // insert a new entry.
  return namespace_storage_key_map_
      .emplace(std::piecewise_construct, std::forward_as_tuple(namespace_id),
               std::forward_as_tuple())
      .first;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetNamespacePrefix(
    const std::string& namespace_id) {
  std::vector<uint8_t> namespace_prefix(kNamespacePrefix,
                                        std::end(kNamespacePrefix));
  namespace_prefix.insert(namespace_prefix.end(), namespace_id.begin(),
                          namespace_id.end());
  namespace_prefix.push_back(kNamespaceStorageKeySeparator);
  return namespace_prefix;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetMapPrefix(int64_t map_number) {
  return GetMapPrefix(NumberToValue(map_number));
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetMapPrefix(
    const std::vector<uint8_t>& map_number_as_bytes) {
  std::vector<uint8_t> map_prefix(kMapIdPrefixBytes,
                                  std::end(kMapIdPrefixBytes));
  map_prefix.insert(map_prefix.end(), map_number_as_bytes.begin(),
                    map_number_as_bytes.end());
  map_prefix.push_back(kNamespaceStorageKeySeparator);
  return map_prefix;
}

}  // namespace storage
