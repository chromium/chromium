// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/storage/dom_storage/session_storage_metadata.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "url/gurl.h"

namespace storage {

namespace {

// Example layout of the database:
// | key                                    | value              |
// |----------------------------------------|--------------------|
// | map-1-a                                | b (a = b in map 1) |
// | ...                                    |                    |
// | namespace-<36 char guid 1>-StorageKey1 | 1 (mapid)          |
// | namespace-<36 char guid 1>-StorageKey2 | 2                  |
// | namespace-<36 char guid 2>-StorageKey1 | 1 (shallow copy)   |
// | namespace-<36 char guid 2>-StorageKey2 | 2 (shallow copy)   |
// | namespace-<36 char guid 3>-StorageKey1 | 3 (deep copy)      |
// | namespace-<36 char guid 3>-StorageKey2 | 2 (shallow copy)   |
// | next-map-id                            | 4                  |
// | version                                | 1                  |
// Example area key:
//   namespace-dabc53e1_8291_4de5_824f_dab8aa69c846-https://example.com/
//
// All number values (map numbers and the version) are string conversions of
// numbers. Map keys are converted to UTF-8 and the values stay as UTF-16.

// This is "map-" (without the quotes).
constexpr const uint8_t kMapIdPrefixBytes[] = {'m', 'a', 'p', '-'};

constexpr const size_t kNamespacePrefixLength =
    std::size(SessionStorageMetadata::kNamespacePrefixBytes);
constexpr const uint8_t kNamespaceStorageKeySeperatorByte = '-';
constexpr const size_t kNamespaceStorageKeySeperatorLength = 1;
constexpr const size_t kPrefixBeforeStorageKeyLength =
    kNamespacePrefixLength + blink::kSessionStorageNamespaceIdLength +
    kNamespaceStorageKeySeperatorLength;

std::string_view Uint8VectorToStringView(const std::vector<uint8_t>& bytes) {
  return std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
}

bool ValueToNumber(const std::vector<uint8_t>& value, int64_t* out) {
  return base::StringToInt64(Uint8VectorToStringView(value), out);
}

std::vector<uint8_t> NumberToValue(int64_t map_number) {
  auto str = base::NumberToString(map_number);
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

constexpr const int64_t SessionStorageMetadata::kMinSessionStorageSchemaVersion;
constexpr const int64_t
    SessionStorageMetadata::kLatestSessionStorageSchemaVersion;
constexpr const int64_t SessionStorageMetadata::kInvalidDatabaseVersion;
constexpr const int64_t SessionStorageMetadata::kInvalidMapId;
constexpr const uint8_t SessionStorageMetadata::kDatabaseVersionBytes[];
constexpr const uint8_t SessionStorageMetadata::kNamespacePrefixBytes[];
constexpr const uint8_t SessionStorageMetadata::kNextMapIdKeyBytes[];

SessionStorageMetadata::MapData::MapData(int64_t map_number,
                                         blink::StorageKey storage_key)
    : number_as_bytes_(NumberToValue(map_number)),
      key_prefix_(SessionStorageMetadata::GetMapPrefix(number_as_bytes_)),
      storage_key_(std::move(storage_key)) {}
SessionStorageMetadata::MapData::~MapData() = default;

SessionStorageMetadata::SessionStorageMetadata() = default;

SessionStorageMetadata::~SessionStorageMetadata() = default;

std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>
SessionStorageMetadata::SetupNewDatabase() {
  next_map_id_ = 0;
  next_map_id_from_namespaces_ = 0;
  namespace_storage_key_map_.clear();

  std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  tasks.push_back(base::BindOnce(
      [](int64_t next_map_id, leveldb::WriteBatch* batch,
         const DomStorageDatabase& db) {
        batch->Put(
            leveldb_env::MakeSlice(base::make_span(kDatabaseVersionBytes)),
            leveldb_env::MakeSlice(LatestDatabaseVersionAsVector()));
        batch->Put(leveldb_env::MakeSlice(base::make_span(kNextMapIdKeyBytes)),
                   leveldb_env::MakeSlice(NumberToValue(next_map_id)));
      },
      next_map_id_));
  return tasks;
}

bool SessionStorageMetadata::ParseDatabaseVersion(
    std::optional<std::vector<uint8_t>> value,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* upgrade_tasks) {
  if (!value) {
    initial_database_version_from_disk_ = 0;
  } else {
    if (!ValueToNumber(value.value(), &initial_database_version_from_disk_)) {
      initial_database_version_from_disk_ = kInvalidDatabaseVersion;
      return false;
    }
    if (initial_database_version_from_disk_ >
        kLatestSessionStorageSchemaVersion) {
      return false;
    }
    if (initial_database_version_from_disk_ ==
        kLatestSessionStorageSchemaVersion) {
      return true;
    }
  }
  if (initial_database_version_from_disk_ < kMinSessionStorageSchemaVersion)
    return false;
  upgrade_tasks->push_back(base::BindOnce([](leveldb::WriteBatch* batch,
                                             const DomStorageDatabase& db) {
    batch->Put(leveldb_env::MakeSlice(base::make_span(kDatabaseVersionBytes)),
               leveldb_env::MakeSlice(LatestDatabaseVersionAsVector()));
  }));
  return true;
}

bool SessionStorageMetadata::ParseNamespaces(
    std::vector<DomStorageDatabase::KeyValuePair> values,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* upgrade_tasks) {
  namespace_storage_key_map_.clear();
  next_map_id_from_namespaces_ = 0;
  // Since the data is ordered, all namespace data is in one spot. This keeps a
  // reference to the last namespace data map to be more efficient.
  std::string last_namespace_id;
  std::map<blink::StorageKey, scoped_refptr<MapData>>* last_namespace = nullptr;
  std::map<int64_t, scoped_refptr<MapData>> maps;
  bool error = false;
  for (const DomStorageDatabase::KeyValuePair& key_value : values) {
    size_t key_size = key_value.key.size();

    std::string_view key_as_string = Uint8VectorToStringView(key_value.key);

    if (key_size < kNamespacePrefixLength) {
      LOG(ERROR) << "Key size is less than prefix length: " << key_as_string;
      error = true;
      break;
    }

    // The key must start with 'namespace-'.
    if (!base::StartsWith(key_as_string,
                          std::string_view(reinterpret_cast<const char*>(
                                               kNamespacePrefixBytes),
                                           kNamespacePrefixLength))) {
      LOG(ERROR) << "Key must start with 'namespace-': " << key_as_string;
      error = true;
      break;
    }

    // Old databases have a dummy 'namespace-' entry.
    if (key_size == kNamespacePrefixLength)
      continue;

    // Check that the prefix is 'namespace-<guid>-
    if (key_size < kPrefixBeforeStorageKeyLength ||
        key_as_string[kPrefixBeforeStorageKeyLength - 1] !=
            static_cast<const char>(kNamespaceStorageKeySeperatorByte)) {
      LOG(ERROR) << "Prefix is not 'namespace-<guid>-': " << key_as_string;
      error = true;
      break;
    }

    // Old databases have a dummy 'namespace-<guid>-' entry.
    if (key_size == kPrefixBeforeStorageKeyLength)
      continue;

    std::string_view namespace_id = key_as_string.substr(
        kNamespacePrefixLength, blink::kSessionStorageNamespaceIdLength);

    std::string_view storage_key_str =
        key_as_string.substr(kPrefixBeforeStorageKeyLength);

    int64_t map_number;
    if (!ValueToNumber(key_value.value, &map_number)) {
      error = true;
      LOG(ERROR) << "Could not parse map number "
                 << Uint8VectorToStringView(key_value.value);
      break;
    }

    if (map_number >= next_map_id_from_namespaces_)
      next_map_id_from_namespaces_ = map_number + 1;

    std::optional<blink::StorageKey> storage_key =
        blink::StorageKey::Deserialize(storage_key_str);
    if (!storage_key) {
      LOG(ERROR) << "Invalid StorageKey " << storage_key_str;
      error = true;
      break;
    }

    if (namespace_id != last_namespace_id) {
      last_namespace_id = std::string(namespace_id);
      DCHECK(namespace_storage_key_map_.find(last_namespace_id) ==
             namespace_storage_key_map_.end());
      last_namespace = &(namespace_storage_key_map_[last_namespace_id]);
    }
    auto map_it = maps.find(map_number);
    if (map_it == maps.end()) {
      map_it =
          maps.emplace(
                  std::piecewise_construct, std::forward_as_tuple(map_number),
                  std::forward_as_tuple(new MapData(map_number, *storage_key)))
              .first;
    }
    map_it->second->IncReferenceCount();

    last_namespace->emplace(
        std::make_pair(std::move(*storage_key), map_it->second));
  }
  if (error) {
    namespace_storage_key_map_.clear();
    next_map_id_from_namespaces_ = 0;
    return false;
  }
  if (next_map_id_ == 0 || next_map_id_ < next_map_id_from_namespaces_)
    next_map_id_ = next_map_id_from_namespaces_;

  // Namespace metadata migration.
  DCHECK_NE(kInvalidDatabaseVersion, initial_database_version_from_disk_);
  if (initial_database_version_from_disk_ == 0) {
    std::vector<DomStorageDatabase::Key> prefix_keys_to_delete;
    for (const auto& entry : maps)
      prefix_keys_to_delete.push_back(entry.second->KeyPrefix());
    // Remove the dummy 'namespaces-' entry.
    upgrade_tasks->push_back(base::BindOnce(
        [](std::vector<DomStorageDatabase::Key> prefix_keys_to_delete,
           leveldb::WriteBatch* batch, const DomStorageDatabase& db) {
          batch->Delete(
              leveldb_env::MakeSlice(base::make_span(kNamespacePrefixBytes)));
          // Remove all the refcount storage.
          for (const auto& key : prefix_keys_to_delete)
            batch->Delete(leveldb_env::MakeSlice(key));
        },
        std::move(prefix_keys_to_delete)));
  }

  return true;
}

void SessionStorageMetadata::ParseNextMapId(
    const std::vector<uint8_t>& map_id) {
  if (!ValueToNumber(map_id, &next_map_id_))
    next_map_id_ = next_map_id_from_namespaces_;
  if (next_map_id_ < next_map_id_from_namespaces_)
    next_map_id_ = next_map_id_from_namespaces_;
}

// static
std::vector<uint8_t> SessionStorageMetadata::LatestDatabaseVersionAsVector() {
  return NumberToValue(kLatestSessionStorageSchemaVersion);
}

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageMetadata::RegisterNewMap(
    NamespaceEntry namespace_entry,
    const blink::StorageKey& storage_key,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks) {
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

  save_tasks->push_back(base::BindOnce(
      [](int64_t new_map_id, DomStorageDatabase::Key storage_key_key,
         DomStorageDatabase::Value storage_key_map_number,
         leveldb::WriteBatch* batch, const DomStorageDatabase& db) {
        batch->Put(leveldb_env::MakeSlice(base::make_span(kNextMapIdKeyBytes)),
                   leveldb_env::MakeSlice(NumberToValue(new_map_id)));
        batch->Put(leveldb_env::MakeSlice(storage_key_key),
                   leveldb_env::MakeSlice(storage_key_map_number));
      },
      next_map_id_, GetAreaKey(namespace_entry->first, storage_key),
      new_map_data->MapNumberAsBytes()));

  return new_map_data;
}

void SessionStorageMetadata::RegisterShallowClonedNamespace(
    NamespaceEntry source_namespace,
    NamespaceEntry destination_namespace,
    std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* save_tasks) {
  DCHECK(save_tasks);
  std::map<blink::StorageKey, scoped_refptr<MapData>>& source_storage_keys =
      source_namespace->second;
  std::map<blink::StorageKey, scoped_refptr<MapData>>&
      destination_storage_keys = destination_namespace->second;
  DCHECK_EQ(0ul, destination_storage_keys.size())
      << "The destination already has data.";

  std::vector<DomStorageDatabase::KeyValuePair> new_entries;
  for (const auto& storage_key_map_pair : source_storage_keys) {
    destination_storage_keys.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(storage_key_map_pair.first),
        std::forward_as_tuple(storage_key_map_pair.second));
    storage_key_map_pair.second->IncReferenceCount();
    new_entries.emplace_back(
        GetAreaKey(destination_namespace->first, storage_key_map_pair.first),
        storage_key_map_pair.second->MapNumberAsBytes());
  }

  save_tasks->push_back(base::BindOnce(
      [](std::vector<DomStorageDatabase::KeyValuePair> new_entries,
         leveldb::WriteBatch* batch, const DomStorageDatabase&) {
        for (const auto& entry : new_entries)
          batch->Put(leveldb_env::MakeSlice(entry.key),
                     leveldb_env::MakeSlice(entry.value));
      },
      std::move(new_entries)));
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
         leveldb::WriteBatch* batch, const DomStorageDatabase& db) {
        for (const auto& prefix : prefixes_to_delete)
          db.DeletePrefixed(prefix, batch);
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

  DomStorageDatabase::Key area_key = GetAreaKey(namespace_id, storage_key);
  std::vector<DomStorageDatabase::Key> prefixes_to_delete;
  DCHECK_GT(map_data->ReferenceCount(), 0);
  map_data->DecReferenceCount();
  if (map_data->ReferenceCount() == 0)
    prefixes_to_delete.push_back(map_data->KeyPrefix());

  ns_entry->second.erase(storage_key_map_it);

  save_tasks->push_back(base::BindOnce(
      [](const DomStorageDatabase::Key& area_key,
         std::vector<DomStorageDatabase::Key> prefixes_to_delete,
         leveldb::WriteBatch* batch, const DomStorageDatabase& db) {
        batch->Delete(leveldb_env::MakeSlice(area_key));
        for (const auto& prefix : prefixes_to_delete)
          db.DeletePrefixed(prefix, batch);
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
  std::vector<uint8_t> namespace_prefix(
      SessionStorageMetadata::kNamespacePrefixBytes,
      std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  namespace_prefix.insert(namespace_prefix.end(), namespace_id.data(),
                          namespace_id.data() + namespace_id.size());
  namespace_prefix.push_back(kNamespaceStorageKeySeperatorByte);
  return namespace_prefix;
}

// static
std::vector<uint8_t> SessionStorageMetadata::GetAreaKey(
    const std::string& namespace_id,
    const blink::StorageKey& storage_key) {
  std::vector<uint8_t> area_key(
      SessionStorageMetadata::kNamespacePrefixBytes,
      std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  area_key.insert(area_key.end(), namespace_id.begin(), namespace_id.end());
  area_key.push_back(kNamespaceStorageKeySeperatorByte);
  std::string storage_key_str = storage_key.Serialize();
  area_key.insert(area_key.end(), storage_key_str.data(),
                  storage_key_str.data() + storage_key_str.size());
  return area_key;
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
  map_prefix.push_back(kNamespaceStorageKeySeperatorByte);
  return map_prefix;
}

}  // namespace storage
