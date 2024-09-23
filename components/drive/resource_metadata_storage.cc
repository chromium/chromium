// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/drive/resource_metadata_storage.h"

#include <stddef.h>

#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_system_core_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace drive {
namespace internal {

namespace {

// Enum to describe DB initialization status.
enum DBInitStatus {
  DB_INIT_SUCCESS,
  DB_INIT_NOT_FOUND,
  DB_INIT_CORRUPTION,
  DB_INIT_IO_ERROR,
  DB_INIT_FAILED,
};

// The name of the DB which stores the metadata.
const base::FilePath::CharType kResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_resource_map.db");

// The name of the DB which couldn't be opened, but is preserved just in case.
const base::FilePath::CharType kPreservedResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_preserved_resource_map.db");

// The name of the DB which couldn't be opened, and was replaced with a new one.
const base::FilePath::CharType kTrashedResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_trashed_resource_map.db");

// Meant to be a character which never happen to be in real IDs.
const char kDBKeyDelimeter = '\0';

// String used as a suffix of a key for a cache entry.
const char kCacheEntryKeySuffix[] = "CACHE";

// String used as a prefix of a key for a resource-ID-to-local-ID entry.
const char kIdEntryKeyPrefix[] = "ID";

// Returns a string to be used as the key for the header.
std::string GetHeaderDBKey() {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append("HEADER");
  return key;
}

// Returns true if |key| is a key for a child entry.
bool IsChildEntryKey(const leveldb::Slice& key) {
  return !key.empty() && key[key.size() - 1] == kDBKeyDelimeter;
}

// Returns true if |key| is a key for a cache entry.
bool IsCacheEntryKey(const leveldb::Slice& key) {
  // A cache entry key should end with |kDBKeyDelimeter + kCacheEntryKeySuffix|.
  const leveldb::Slice expected_suffix(kCacheEntryKeySuffix,
                                       std::size(kCacheEntryKeySuffix) - 1);
  if (key.size() < 1 + expected_suffix.size() ||
      key[key.size() - expected_suffix.size() - 1] != kDBKeyDelimeter)
    return false;

  const leveldb::Slice key_substring(
      key.data() + key.size() - expected_suffix.size(), expected_suffix.size());
  return key_substring.compare(expected_suffix) == 0;
}

// Returns ID extracted from a cache entry key.
std::string GetIdFromCacheEntryKey(const leveldb::Slice& key) {
  DCHECK(IsCacheEntryKey(key));
  // Drop the suffix |kDBKeyDelimeter + kCacheEntryKeySuffix| from the key.
  const size_t kSuffixLength = std::size(kCacheEntryKeySuffix) - 1;
  const int id_length = key.size() - 1 - kSuffixLength;
  return std::string(key.data(), id_length);
}

// Returns a string to be used as a key for a resource-ID-to-local-ID entry.
std::string GetIdEntryKey(const std::string& resource_id) {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append(kIdEntryKeyPrefix);
  key.push_back(kDBKeyDelimeter);
  key.append(resource_id);
  return key;
}

// Returns true if |key| is a key for a resource-ID-to-local-ID entry.
bool IsIdEntryKey(const leveldb::Slice& key) {
  // A resource-ID-to-local-ID entry key should start with
  // |kDBKeyDelimeter + kIdEntryKeyPrefix + kDBKeyDelimeter|.
  const leveldb::Slice expected_prefix(kIdEntryKeyPrefix,
                                       std::size(kIdEntryKeyPrefix) - 1);
  if (key.size() < 2 + expected_prefix.size())
    return false;
  const leveldb::Slice key_substring(key.data() + 1, expected_prefix.size());
  return key[0] == kDBKeyDelimeter &&
      key_substring.compare(expected_prefix) == 0 &&
      key[expected_prefix.size() + 1] == kDBKeyDelimeter;
}

// Returns the resource ID extracted from a resource-ID-to-local-ID entry key.
std::string GetResourceIdFromIdEntryKey(const leveldb::Slice& key) {
  DCHECK(IsIdEntryKey(key));
  // Drop the prefix |kDBKeyDelimeter + kIdEntryKeyPrefix + kDBKeyDelimeter|
  // from the key.
  const size_t kPrefixLength = std::size(kIdEntryKeyPrefix) - 1;
  const int offset = kPrefixLength + 2;
  return std::string(key.data() + offset, key.size() - offset);
}

// Converts leveldb::Status to DBInitStatus.
DBInitStatus LevelDBStatusToDBInitStatus(const leveldb::Status& status) {
  if (status.ok())
    return DB_INIT_SUCCESS;
  if (status.IsNotFound())
    return DB_INIT_NOT_FOUND;
  if (status.IsCorruption())
    return DB_INIT_CORRUPTION;
  if (status.IsIOError())
    return DB_INIT_IO_ERROR;
  return DB_INIT_FAILED;
}

// Converts leveldb::Status to FileError.
FileError LevelDBStatusToFileError(const leveldb::Status& status) {
  if (status.ok())
    return FILE_ERROR_OK;
  if (status.IsNotFound())
    return FILE_ERROR_NOT_FOUND;
  if (leveldb_env::IndicatesDiskFull(status))
    return FILE_ERROR_NO_LOCAL_SPACE;
  return FILE_ERROR_FAILED;
}

ResourceMetadataHeader GetDefaultHeaderEntry() {
  ResourceMetadataHeader header;
  header.set_version(ResourceMetadataStorage::kDBVersion);
  return header;
}

bool MoveIfPossible(const base::FilePath& from, const base::FilePath& to) {
  return !base::PathExists(from) || base::Move(from, to);
}

bool UpgradeOldDBVersions6To10(leveldb::DB* resource_map) {
  // Cache entries can be reused.
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::unique_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

  leveldb::WriteBatch batch;
  // First, remove all entries.
  for (it->SeekToFirst(); it->Valid(); it->Next())
    batch.Delete(it->key());

  // Put ID entries and cache entries.
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (!IsCacheEntryKey(it->key()))
      continue;

    FileCacheEntry cache_entry;
    if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
      return false;

    // The resource ID might be in old WAPI format. We need to canonicalize
    // to the format of API service currently in use.
    const std::string& id = GetIdFromCacheEntryKey(it->key());
    const std::string& id_new = util::CanonicalizeResourceId(id);

    // Before v11, resource ID was directly used as local ID. Such entries
    // can be migrated by adding an identity ID mapping.
    batch.Put(GetIdEntryKey(id_new), id_new);

    // Put cache state into a ResourceEntry.
    ResourceEntry entry;
    entry.set_local_id(id_new);
    entry.set_resource_id(id_new);
    *entry.mutable_file_specific_info()->mutable_cache_state() = cache_entry;

    std::string serialized_entry;
    if (!entry.SerializeToString(&serialized_entry)) {
      DLOG(ERROR) << "Failed to serialize the entry: " << id;
      return false;
    }
    batch.Put(id_new, serialized_entry);
  }
  if (!it->status().ok())
    return false;

  // Put header with the latest version number. This also clears
  // largest_changestamp and triggers refresh of metadata.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersion11(leveldb::DB* resource_map) {
  // Cache and ID map entries are reusable.
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::unique_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

  // First, get the set of local IDs associated with cache entries.
  std::set<std::string> cached_entry_ids;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (IsCacheEntryKey(it->key()))
      cached_entry_ids.insert(GetIdFromCacheEntryKey(it->key()));
  }
  if (!it->status().ok())
    return false;

  // Remove all entries except used ID entries.
  leveldb::WriteBatch batch;
  std::map<std::string, std::string> local_id_to_resource_id;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const bool is_used_id = IsIdEntryKey(it->key()) &&
                            cached_entry_ids.count(it->value().ToString());
    if (is_used_id) {
      local_id_to_resource_id[it->value().ToString()] =
          GetResourceIdFromIdEntryKey(it->key());
    } else {
      batch.Delete(it->key());
    }
  }
  if (!it->status().ok())
    return false;

  // Put cache entries.
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (!IsCacheEntryKey(it->key()))
      continue;

    const std::string& id = GetIdFromCacheEntryKey(it->key());
    const auto iter_resource_id = local_id_to_resource_id.find(id);
    if (iter_resource_id == local_id_to_resource_id.end())
      continue;

    FileCacheEntry cache_entry;
    if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
      return false;

    // Put cache state into a ResourceEntry.
    ResourceEntry entry;
    entry.set_local_id(id);
    entry.set_resource_id(iter_resource_id->second);
    *entry.mutable_file_specific_info()->mutable_cache_state() = cache_entry;

    std::string serialized_entry;
    if (!entry.SerializeToString(&serialized_entry)) {
      DLOG(ERROR) << "Failed to serialize the entry: " << id;
      return false;
    }
    batch.Put(id, serialized_entry);
  }
  if (!it->status().ok())
    return false;

  // Put header with the latest version number. This also clears
  // largest_changestamp and triggers refresh of metadata.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersion12(leveldb::DB* resource_map) {
  // Reuse all entries.
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::unique_ptr<leveldb::Iterator> it(resource_map->NewIterator(options));

  // First, get local ID to resource ID map.
  std::map<std::string, std::string> local_id_to_resource_id;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (IsIdEntryKey(it->key())) {
      local_id_to_resource_id[it->value().ToString()] =
          GetResourceIdFromIdEntryKey(it->key());
    }
  }
  if (!it->status().ok())
    return false;

  leveldb::WriteBatch batch;
  // Merge cache entries to ResourceEntry.
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (!IsCacheEntryKey(it->key()))
      continue;

    const std::string& id = GetIdFromCacheEntryKey(it->key());

    FileCacheEntry cache_entry;
    if (!cache_entry.ParseFromArray(it->value().data(), it->value().size()))
      return false;

    std::string serialized_entry;
    leveldb::Status status =
        resource_map->Get(options, leveldb::Slice(id), &serialized_entry);

    const auto iter_resource_id = local_id_to_resource_id.find(id);

    // No need to keep cache-only entries without resource ID.
    if (status.IsNotFound() &&
        iter_resource_id == local_id_to_resource_id.end())
      continue;

    ResourceEntry entry;
    if (status.ok()) {
      if (!entry.ParseFromString(serialized_entry))
        return false;
    } else if (status.IsNotFound()) {
      entry.set_local_id(id);
      entry.set_resource_id(iter_resource_id->second);
    } else {
      DLOG(ERROR) << "Failed to get the entry: " << id;
      return false;
    }
    *entry.mutable_file_specific_info()->mutable_cache_state() = cache_entry;

    if (!entry.SerializeToString(&serialized_entry)) {
      DLOG(ERROR) << "Failed to serialize the entry: " << id;
      return false;
    }
    batch.Delete(it->key());
    batch.Put(id, serialized_entry);
  }
  if (!it->status().ok())
    return false;

  // Put header with the latest version number. This also clears
  // largest_changestamp and triggers refresh of metadata.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersion13(leveldb::DB* resource_map) {
  // Before r272134, UpgradeOldDB() was not deleting unused ID entries.
  // Delete unused ID entries to fix crbug.com/374648.
  std::set<std::string> used_ids;

  std::unique_ptr<leveldb::Iterator> it(
      resource_map->NewIterator(leveldb::ReadOptions()));
  it->Seek(leveldb::Slice(GetHeaderDBKey()));
  it->Next();
  for (; it->Valid(); it->Next()) {
    if (IsCacheEntryKey(it->key()))
      used_ids.insert(GetIdFromCacheEntryKey(it->key()));
    else if (!IsChildEntryKey(it->key()) && !IsIdEntryKey(it->key()))
      used_ids.insert(it->key().ToString());
  }
  if (!it->status().ok())
    return false;

  leveldb::WriteBatch batch;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (IsIdEntryKey(it->key()) && !used_ids.count(it->value().ToString()))
      batch.Delete(it->key());
  }
  if (!it->status().ok())
    return false;

  // Put header with the latest version number. This also clears
  // largest_changestamp and triggers refresh of metadata.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersion14(leveldb::DB* resource_map) {
  // Just need to clear largest_changestamp.
  // Put header with the latest version number.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  leveldb::WriteBatch batch;
  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersion15(leveldb::DB* resource_map) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  leveldb::WriteBatch batch;

  std::unique_ptr<leveldb::Iterator> it(
      resource_map->NewIterator(read_options));

  it->SeekToFirst();
  ResourceMetadataHeader header;

  if (!it->Valid() || it->key() != GetHeaderDBKey()) {
    DLOG(ERROR) << "Header not detected.";
    return false;
  }

  if (!header.ParseFromArray(it->value().data(), it->value().size())) {
    DLOG(ERROR) << "Could not parse header.";
    return false;
  }

  header.set_version(ResourceMetadataStorage::kDBVersion);
  header.set_start_page_token(drive::util::ConvertChangestampToStartPageToken(
      header.largest_changestamp()));
  std::string serialized_header;
  header.SerializeToString(&serialized_header);
  batch.Put(GetHeaderDBKey(), serialized_header);

  for (it->Next(); it->Valid(); it->Next()) {
    if (IsIdEntryKey(it->key()))
      continue;

    ResourceEntry entry;
    if (!entry.ParseFromArray(it->value().data(), it->value().size()))
      return false;

    if (entry.has_directory_specific_info()) {
      int64_t changestamp = entry.directory_specific_info().changestamp();
      entry.mutable_directory_specific_info()->set_start_page_token(
          drive::util::ConvertChangestampToStartPageToken(changestamp));

      std::string serialized_entry;
      if (!entry.SerializeToString(&serialized_entry)) {
        DLOG(ERROR) << "Failed to serialize the entry";
        return false;
      }

      batch.Put(entry.local_id(), serialized_entry);
    }
  }

  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

bool UpgradeOldDBVersions16To18(leveldb::DB* resource_map) {
  // From 15->16, the field |alternate_url| was moved from FileSpecificData
  // to ResourceEntry. Since it isn't saved for directories, we need to do a
  // full fetch to get the |alternate_url| fetched for each directory.
  // Put a new header with the latest version number, and clear the start page
  // token.
  std::string serialized_header;
  if (!GetDefaultHeaderEntry().SerializeToString(&serialized_header))
    return false;

  leveldb::WriteBatch batch;
  batch.Put(GetHeaderDBKey(), serialized_header);
  return resource_map->Write(leveldb::WriteOptions(), &batch).ok();
}

}  // namespace

ResourceMetadataStorage::Iterator::Iterator(
    std::unique_ptr<leveldb::Iterator> it)
    : it_(std::move(it)) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(it_);

  // Skip the header entry.
  // Note: The header entry comes before all other entries because its key
  // starts with kDBKeyDelimeter. (i.e. '\0')
  it_->Seek(leveldb::Slice(GetHeaderDBKey()));

  Advance();
}

ResourceMetadataStorage::Iterator::~Iterator() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
}

bool ResourceMetadataStorage::Iterator::IsAtEnd() const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return !it_->Valid();
}

std::string ResourceMetadataStorage::Iterator::GetID() const {
  return it_->key().ToString();
}

const ResourceEntry& ResourceMetadataStorage::Iterator::GetValue() const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!IsAtEnd());
  return entry_;
}

void ResourceMetadataStorage::Iterator::Advance() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!IsAtEnd());

  for (it_->Next() ; it_->Valid(); it_->Next()) {
    if (!IsChildEntryKey(it_->key()) &&
        !IsIdEntryKey(it_->key()) &&
        entry_.ParseFromArray(it_->value().data(), it_->value().size())) {
      break;
    }
  }
}

bool ResourceMetadataStorage::Iterator::HasError() const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return !it_->status().ok();
}

// static
bool ResourceMetadataStorage::UpgradeOldDB(
    const base::FilePath& directory_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath resource_map_path =
      directory_path.Append(kResourceMapDBName);
  const base::FilePath preserved_resource_map_path =
      directory_path.Append(kPreservedResourceMapDBName);

  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;

  if (base::PathExists(preserved_resource_map_path)) {
    // Preserved DB is found. The previous attempt to create a new DB should not
    // be successful. Discard the imperfect new DB and restore the old DB.
    leveldb::Status status =
        leveldb_chrome::DeleteDB(resource_map_path, options);
    if (!status.ok()) {
      LOG(ERROR) << "ERROR deleting " << resource_map_path
                 << ", err:" << status.ToString();
      return false;
    }
    if (!base::Move(preserved_resource_map_path, resource_map_path))
      return false;
  }

  if (!base::PathExists(resource_map_path))
    return false;

  // Open DB.
  std::unique_ptr<leveldb::DB> resource_map;
  leveldb::Status status = leveldb_env::OpenDB(
      options, resource_map_path.AsUTF8Unsafe(), &resource_map);
  if (!status.ok())
    return false;

  // Check DB version.
  std::string serialized_header;
  ResourceMetadataHeader header;
  if (!resource_map->Get(leveldb::ReadOptions(),
                         leveldb::Slice(GetHeaderDBKey()),
                         &serialized_header).ok() ||
      !header.ParseFromString(serialized_header))
    return false;

  switch (header.version()) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      return false;  // Too old, nothing can be done.
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
      return UpgradeOldDBVersions6To10(resource_map.get());
    case 11:
      return UpgradeOldDBVersion11(resource_map.get());
    case 12:
      return UpgradeOldDBVersion12(resource_map.get());
    case 13:
      return UpgradeOldDBVersion13(resource_map.get());
    case 14:
      return UpgradeOldDBVersion14(resource_map.get());
    case 15:
      return UpgradeOldDBVersion15(resource_map.get());
    case 16:
    case 17:
    case 18:
      return UpgradeOldDBVersions16To18(resource_map.get());
    case kDBVersion:
      static_assert(
          kDBVersion == 19,
          "database version and this function must be updated together");
      return true;
    default:
      LOG(WARNING) << "Unexpected DB version: " << header.version();
      return false;
  }
}

ResourceMetadataStorage::ResourceMetadataStorage(
    const base::FilePath& directory_path,
    base::SequencedTaskRunner* blocking_task_runner)
    : directory_path_(directory_path),
      cache_file_scan_is_needed_(true),
      blocking_task_runner_(blocking_task_runner) {
}

bool ResourceMetadataStorage::Initialize() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  resource_map_.reset();

  const base::FilePath resource_map_path =
      directory_path_.Append(kResourceMapDBName);
  const base::FilePath preserved_resource_map_path =
      directory_path_.Append(kPreservedResourceMapDBName);
  const base::FilePath trashed_resource_map_path =
      directory_path_.Append(kTrashedResourceMapDBName);

  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;

  // Discard unneeded DBs.
  if (!leveldb_chrome::DeleteDB(preserved_resource_map_path, options).ok() ||
      !leveldb_chrome::DeleteDB(trashed_resource_map_path, options).ok()) {
    LOG(ERROR) << "Failed to remove unneeded DBs.";
    return false;
  }

  // Try to open the existing DB.
  DBInitStatus open_existing_result = DB_INIT_NOT_FOUND;
  leveldb::Status status;
  if (base::PathExists(resource_map_path)) {
    status = leveldb_env::OpenDB(options, resource_map_path.AsUTF8Unsafe(),
                                 &resource_map_);
    open_existing_result = LevelDBStatusToDBInitStatus(status);
  }

  if (open_existing_result == DB_INIT_SUCCESS) {
    // Check the validity of existing DB.
    int db_version = -1;
    ResourceMetadataHeader header;
    if (GetHeader(&header) == FILE_ERROR_OK)
      db_version = header.version();

    bool should_discard_db = true;
    if (db_version != kDBVersion) {
      DVLOG(1) << "Reject incompatible DB.";
    } else if (!CheckValidity()) {
      LOG(ERROR) << "Reject invalid DB.";
    } else {
      should_discard_db = false;
    }

    if (should_discard_db)
      resource_map_.reset();
    else
      cache_file_scan_is_needed_ = false;
  }

  // Failed to open the existing DB, create new DB.
  if (!resource_map_) {
    // Move the existing DB to the preservation path. The moved old DB is
    // deleted once the new DB creation succeeds, or is restored later in
    // UpgradeOldDB() when the creation fails.
    MoveIfPossible(resource_map_path, preserved_resource_map_path);

    // Create DB.
    options = leveldb_env::Options();
    options.max_open_files = 0;  // Use minimum.
    options.create_if_missing = true;
    options.error_if_exists = true;

    status = leveldb_env::OpenDB(options, resource_map_path.AsUTF8Unsafe(),
                                 &resource_map_);
    if (status.ok()) {
      // Set up header and trash the old DB.
      if (PutHeader(GetDefaultHeaderEntry()) != FILE_ERROR_OK ||
          !MoveIfPossible(preserved_resource_map_path,
                          trashed_resource_map_path)) {
        resource_map_.reset();
      }
    } else {
      LOG(ERROR) << "Failed to create resource map DB: " << status.ToString();
    }
  }
  return !!resource_map_;
}

void ResourceMetadataStorage::Destroy() {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ResourceMetadataStorage::DestroyOnBlockingPool,
                                base::Unretained(this)));
}

void ResourceMetadataStorage::RecoverCacheInfoFromTrashedResourceMap(
    RecoveredCacheInfoMap* out_info) {
  const base::FilePath trashed_resource_map_path =
      directory_path_.Append(kTrashedResourceMapDBName);

  if (!base::PathExists(trashed_resource_map_path))
    return;

  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = false;
  options.reuse_logs = false;

  // Trashed DB may be broken, repair it first.
  leveldb::Status status;
  status = leveldb::RepairDB(trashed_resource_map_path.AsUTF8Unsafe(), options);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to repair trashed DB: " << status.ToString();
    return;
  }

  // Open it.
  std::unique_ptr<leveldb::DB> resource_map;
  status = leveldb_env::OpenDB(
      options, trashed_resource_map_path.AsUTF8Unsafe(), &resource_map);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open trashed DB: " << status.ToString();
    return;
  }

  // Check DB version.
  std::string serialized_header;
  ResourceMetadataHeader header;
  if (!resource_map->Get(leveldb::ReadOptions(),
                         leveldb::Slice(GetHeaderDBKey()),
                         &serialized_header).ok() ||
      !header.ParseFromString(serialized_header) ||
      header.version() != kDBVersion) {
    LOG(ERROR) << "Incompatible DB version: " << header.version();
    return;
  }

  // Collect cache entries.
  std::unique_ptr<leveldb::Iterator> it(
      resource_map->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (!IsChildEntryKey(it->key()) &&
        !IsIdEntryKey(it->key())) {
      const std::string id = it->key().ToString();
      ResourceEntry entry;
      if (entry.ParseFromArray(it->value().data(), it->value().size()) &&
          entry.file_specific_info().has_cache_state()) {
        RecoveredCacheInfo* info = &(*out_info)[id];
        info->is_dirty = entry.file_specific_info().cache_state().is_dirty();
        info->md5 = entry.file_specific_info().cache_state().md5();
        info->title = entry.title();
      }
    }
  }
}

FileError ResourceMetadataStorage::SetLargestChangestamp(
    int64_t largest_changestamp) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  header.set_largest_changestamp(largest_changestamp);
  return PutHeader(header);
}

FileError ResourceMetadataStorage::GetLargestChangestamp(
    int64_t* largest_changestamp) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  *largest_changestamp = header.largest_changestamp();
  return FILE_ERROR_OK;
}

FileError ResourceMetadataStorage::GetStartPageToken(
    std::string* start_page_token) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  *start_page_token = header.start_page_token();
  return FILE_ERROR_OK;
}

FileError ResourceMetadataStorage::SetStartPageToken(
    const std::string& start_page_token) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ResourceMetadataHeader header;
  FileError error = GetHeader(&header);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "Failed to get the header.";
    return error;
  }
  header.set_start_page_token(start_page_token);
  return PutHeader(header);
}

FileError ResourceMetadataStorage::PutEntry(const ResourceEntry& entry) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const std::string& id = entry.local_id();
  DCHECK(!id.empty());

  // Try to get existing entry.
  std::string serialized_entry;
  leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                              leveldb::Slice(id),
                                              &serialized_entry);
  if (!status.ok() && !status.IsNotFound())  // Unexpected errors.
    return LevelDBStatusToFileError(status);

  ResourceEntry old_entry;
  if (status.ok() && !old_entry.ParseFromString(serialized_entry))
    return FILE_ERROR_FAILED;

  // Construct write batch.
  leveldb::WriteBatch batch;

  // Remove from the old parent.
  if (!old_entry.parent_local_id().empty()) {
    batch.Delete(GetChildEntryKey(old_entry.parent_local_id(),
                                  old_entry.base_name()));
  }
  // Add to the new parent.
  if (!entry.parent_local_id().empty())
    batch.Put(GetChildEntryKey(entry.parent_local_id(), entry.base_name()), id);

  // Refresh resource-ID-to-local-ID mapping entry.
  if (old_entry.resource_id() != entry.resource_id()) {
    // Resource ID should not change.
    DCHECK(old_entry.resource_id().empty() || entry.resource_id().empty());

    if (!old_entry.resource_id().empty())
      batch.Delete(GetIdEntryKey(old_entry.resource_id()));
    if (!entry.resource_id().empty())
      batch.Put(GetIdEntryKey(entry.resource_id()), id);
  }

  // Put the entry itself.
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry: " << id;
    return FILE_ERROR_FAILED;
  }
  batch.Put(id, serialized_entry);

  status = resource_map_->Write(leveldb::WriteOptions(), &batch);
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetEntry(const std::string& id,
                                            ResourceEntry* out_entry) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                                    leveldb::Slice(id),
                                                    &serialized_entry);
  if (!status.ok())
    return LevelDBStatusToFileError(status);
  if (!out_entry->ParseFromString(serialized_entry))
    return FILE_ERROR_FAILED;
  return FILE_ERROR_OK;
}

FileError ResourceMetadataStorage::RemoveEntry(const std::string& id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!id.empty());

  ResourceEntry entry;
  FileError error = GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  leveldb::WriteBatch batch;

  // Remove from the parent.
  if (!entry.parent_local_id().empty())
    batch.Delete(GetChildEntryKey(entry.parent_local_id(), entry.base_name()));

  // Remove resource ID-local ID mapping entry.
  if (!entry.resource_id().empty())
    batch.Delete(GetIdEntryKey(entry.resource_id()));

  // Remove the entry itself.
  batch.Delete(id);

  const leveldb::Status status = resource_map_->Write(leveldb::WriteOptions(),
                                                      &batch);
  return LevelDBStatusToFileError(status);
}

std::unique_ptr<ResourceMetadataStorage::Iterator>
ResourceMetadataStorage::GetIterator() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  return std::make_unique<Iterator>(std::move(it));
}

FileError ResourceMetadataStorage::GetChild(const std::string& parent_id,
                                            const std::string& child_name,
                                            std::string* child_id) const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!parent_id.empty());
  DCHECK(!child_name.empty());

  const leveldb::Status status =
      resource_map_->Get(
          leveldb::ReadOptions(),
          leveldb::Slice(GetChildEntryKey(parent_id, child_name)),
          child_id);
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetChildren(
    const std::string& parent_id,
    std::vector<std::string>* children) const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!parent_id.empty());

  // Iterate over all entries with keys starting with |parent_id|.
  std::unique_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  for (it->Seek(parent_id);
       it->Valid() && it->key().starts_with(leveldb::Slice(parent_id));
       it->Next()) {
    if (IsChildEntryKey(it->key()))
      children->push_back(it->value().ToString());
  }
  return LevelDBStatusToFileError(it->status());
}

FileError ResourceMetadataStorage::GetIdByResourceId(
    const std::string& resource_id,
    std::string* out_id) const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!resource_id.empty());

  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetIdEntryKey(resource_id)),
      out_id);
  return LevelDBStatusToFileError(status);
}

ResourceMetadataStorage::RecoveredCacheInfo::RecoveredCacheInfo()
    : is_dirty(false) {}

ResourceMetadataStorage::RecoveredCacheInfo::~RecoveredCacheInfo() = default;

ResourceMetadataStorage::~ResourceMetadataStorage() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
}

void ResourceMetadataStorage::DestroyOnBlockingPool() {
  delete this;
}

// static
std::string ResourceMetadataStorage::GetChildEntryKey(
    const std::string& parent_id,
    const std::string& child_name) {
  DCHECK(!parent_id.empty());
  DCHECK(!child_name.empty());

  std::string key = parent_id;
  key.push_back(kDBKeyDelimeter);
  key.append(child_name);
  key.push_back(kDBKeyDelimeter);
  return key;
}

FileError ResourceMetadataStorage::PutHeader(
    const ResourceMetadataHeader& header) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string serialized_header;
  if (!header.SerializeToString(&serialized_header)) {
    DLOG(ERROR) << "Failed to serialize the header";
    return FILE_ERROR_FAILED;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      leveldb::Slice(serialized_header));
  return LevelDBStatusToFileError(status);
}

FileError ResourceMetadataStorage::GetHeader(
    ResourceMetadataHeader* header) const {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string serialized_header;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      &serialized_header);
  if (!status.ok())
    return LevelDBStatusToFileError(status);
  return header->ParseFromString(serialized_header) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

bool ResourceMetadataStorage::CheckValidity() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Perform read with checksums verification enabled.
  leveldb::ReadOptions options;
  options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> it(resource_map_->NewIterator(options));
  it->SeekToFirst();

  // DB is organized like this:
  //
  // <key>                          : <value>
  // "\0HEADER"                     : ResourceMetadataHeader
  // "\0ID\0|resource ID 1|"        : Local ID associated to resource ID 1.
  // "\0ID\0|resource ID 2|"        : Local ID associated to resource ID 2.
  // ...
  // "|ID of A|"                    : ResourceEntry for entry A.
  // "|ID of A|\0|child name 1|\0"  : ID of the 1st child entry of entry A.
  // "|ID of A|\0|child name 2|\0"  : ID of the 2nd child entry of entry A.
  // ...
  // "|ID of A|\0|child name n|\0"  : ID of the nth child entry of entry A.
  // "|ID of B|"                    : ResourceEntry for entry B.
  // ...

  // Check the header.
  ResourceMetadataHeader header;
  if (!it->Valid() ||
      it->key() != GetHeaderDBKey() ||  // Header entry must come first.
      !header.ParseFromArray(it->value().data(), it->value().size()) ||
      header.version() != kDBVersion) {
    DLOG(ERROR) << "Invalid header detected. version = " << header.version();
    return false;
  }

  // First scan. Remember relationships between IDs.
  typedef std::unordered_map<std::string, std::string> KeyToIdMapping;
  KeyToIdMapping local_id_to_resource_id_map;
  KeyToIdMapping child_key_to_local_id_map;
  std::set<std::string> resource_entries;
  std::string first_resource_entry_key;
  for (it->Next(); it->Valid(); it->Next()) {
    if (IsChildEntryKey(it->key())) {
      child_key_to_local_id_map[it->key().ToString()] = it->value().ToString();
      continue;
    }

    if (IsIdEntryKey(it->key())) {
      const auto result = local_id_to_resource_id_map.insert(std::make_pair(
          it->value().ToString(),
          GetResourceIdFromIdEntryKey(it->key().ToString())));
      // Check that no local ID is associated with more than one resource ID.
      if (!result.second) {
        DLOG(ERROR) << "Broken ID entry.";
        return false;
      }
      continue;
    }

    // Remember the key of the first resource entry record, so the second scan
    // can start from this point.
    if (first_resource_entry_key.empty())
      first_resource_entry_key = it->key().ToString();

    resource_entries.insert(it->key().ToString());
  }

  // Second scan. Verify relationships and resource entry correctness.
  size_t num_entries_with_parent = 0;
  ResourceEntry entry;
  for (it->Seek(first_resource_entry_key); it->Valid(); it->Next()) {
    if (IsChildEntryKey(it->key()))
      continue;

    if (!entry.ParseFromArray(it->value().data(), it->value().size())) {
      DLOG(ERROR) << "Broken entry detected.";
      return false;
    }

    // Resource-ID-to-local-ID mapping without entry for the local ID is OK,
    // but if it exists, then the resource ID must be consistent.
    const auto mapping_it =
        local_id_to_resource_id_map.find(it->key().ToString());
    if (mapping_it != local_id_to_resource_id_map.end() &&
        entry.resource_id() != mapping_it->second) {
      DLOG(ERROR) << "Broken ID entry.";
      return false;
    }

    // If the parent is referenced, then confirm that it exists and check the
    // parent-child relationships.
    if (!entry.parent_local_id().empty()) {
      const auto parent_mapping_it =
          resource_entries.find(entry.parent_local_id());
      if (parent_mapping_it == resource_entries.end()) {
        DLOG(ERROR) << "Parent entry not found.";
        return false;
      }

      // Check if parent-child relationship is stored correctly.
      const auto child_mapping_it = child_key_to_local_id_map.find(
          GetChildEntryKey(entry.parent_local_id(), entry.base_name()));
      if (child_mapping_it == child_key_to_local_id_map.end() ||
          leveldb::Slice(child_mapping_it->second) != it->key()) {
        DLOG(ERROR) << "Child map is broken.";
        return false;
      }
      ++num_entries_with_parent;
    }
  }

  if (!it->status().ok()) {
    DLOG(ERROR) << "Error during checking resource map. status = "
                << it->status().ToString();
    return false;
  }

  if (child_key_to_local_id_map.size() != num_entries_with_parent) {
    DLOG(ERROR) << "Child entry count mismatch.";
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace drive
