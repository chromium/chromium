// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/leveldb_database.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb_proto {

namespace {

// Covers 8MB block cache,
const int kMaxApproxMemoryUseMB = 16;

}  // namespace

bool PrefixStopCallback(const std::string& prefix, const std::string& key) {
  return base::StartsWith(key, prefix, base::CompareCase::SENSITIVE);
}

LevelDB::LevelDB(const char* client_name) : open_histogram_(nullptr) {
  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      std::string("LevelDB.Open.") + client_name, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  approx_memtable_mem_histogram_ = base::LinearHistogram::FactoryGet(
      std::string("LevelDB.ApproximateMemTableMemoryUse.") + client_name, 1,
      kMaxApproxMemoryUseMB * 1048576, kMaxApproxMemoryUseMB * 4,
      base::Histogram::kUmaTargetedHistogramFlag);
}

LevelDB::~LevelDB() {
  DFAKE_SCOPED_LOCK(thread_checker_);
}

bool LevelDB::Init(const base::FilePath& database_dir,
                   const leveldb_env::Options& options) {
  auto status = Init(database_dir, options, true /* destroy_on_corruption */);
  return status.ok();
}

leveldb::Status LevelDB::Init(const base::FilePath& database_dir,
                              const leveldb_env::Options& options,
                              bool destroy_on_corruption) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  database_dir_ = database_dir;
  open_options_ = options;

  bool in_mem = database_dir.empty();
  if (in_mem) {
    env_ = leveldb_chrome::NewMemEnv("LevelDB");
    open_options_.env = env_.get();
  }

  const std::string path = database_dir.AsUTF8Unsafe();

  leveldb::Status status = leveldb_env::OpenDB(open_options_, path, &db_);
  if (open_histogram_)
    open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
  if (destroy_on_corruption && status.IsCorruption()) {
    auto destroy_status = Destroy();
    if (!destroy_status.ok())
      return status;
    status = leveldb_env::OpenDB(open_options_, path, &db_);
    // Intentionally do not log the status of the second open. Doing so destroys
    // the meaning of corruptions/open which is an important statistic.
  }

  if (status.ok()) {
    if (!in_mem) {
      // Record the approximate memory usage of this DB right after init.
      // This should just be the size of the MemTable since we haven't done any
      // reads/writes and the block cache should be empty.
      uint64_t approx_mem = 0;
      std::string usage_string;
      if (GetApproximateMemoryUse(&approx_mem)) {
        approx_memtable_mem_histogram_->Add(
            approx_mem -
            leveldb_chrome::GetSharedBrowserBlockCache()->TotalCharge());
      }
    }
    // Don't log warnings when result is InvalidArgument and create_if_missing
    // is false, as this means the DB file doesn't exist and the client didn't
    // ask to create a new one.
  } else if (!(status.IsInvalidArgument() &&
               !open_options_.create_if_missing)) {
    LOG(WARNING) << "Unable to open " << database_dir.value() << ": "
                 << status.ToString();
  }
  return status;
}

bool LevelDB::Save(const base::StringPairs& entries_to_save,
                   const std::vector<std::string>& keys_to_remove,
                   leveldb::Status* status) {
  DCHECK(status);
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::WriteBatch updates;
  for (const auto& pair : entries_to_save)
    updates.Put(leveldb::Slice(pair.first), leveldb::Slice(pair.second));

  for (const auto& key : keys_to_remove)
    updates.Delete(leveldb::Slice(key));

  leveldb::WriteOptions options;
  options.sync = true;

  *status = db_->Write(options, &updates);
  if (status->ok())
    return true;

  DLOG(WARNING) << "Failed writing leveldb_proto entries: "
                << status->ToString();
  return false;
}

bool LevelDB::UpdateWithRemoveFilter(const base::StringPairs& entries_to_save,
                                     const KeyFilter& delete_key_filter,
                                     leveldb::Status* status) {
  return UpdateWithRemoveFilter(entries_to_save, delete_key_filter,
                                std::string(), status);
}

bool LevelDB::UpdateWithRemoveFilter(const base::StringPairs& entries_to_save,
                                     const KeyFilter& delete_key_filter,
                                     const std::string& target_prefix,
                                     leveldb::Status* status) {
  DCHECK(status);
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::WriteBatch updates;
  for (const auto& pair : entries_to_save)
    updates.Put(leveldb::Slice(pair.first), leveldb::Slice(pair.second));

  leveldb::Slice target(target_prefix);
  if (!delete_key_filter.is_null()) {
    leveldb::ReadOptions read_options;
    std::unique_ptr<leveldb::Iterator> db_iterator(
        db_->NewIterator(read_options));
    for (db_iterator->Seek(target);
         db_iterator->Valid() && db_iterator->key().starts_with(target);
         db_iterator->Next()) {
      leveldb::Slice key_slice = db_iterator->key();
      std::string key(key_slice.data(), key_slice.size());
      if (delete_key_filter.Run(key))
        updates.Delete(leveldb::Slice(key));
    }
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  *status = db_->Write(write_options, &updates);
  if (status->ok())
    return true;

  DLOG(WARNING) << "Failed deleting leveldb_proto entries: "
                << status->ToString();
  return false;
}

bool LevelDB::Load(std::vector<std::string>* entries) {
  return LoadWithFilter(KeyFilter(), entries);
}

bool LevelDB::LoadWithFilter(const KeyFilter& filter,
                             std::vector<std::string>* entries) {
  return LoadWithFilter(filter, entries, leveldb::ReadOptions(), std::string());
}

bool LevelDB::LoadWithFilter(const KeyFilter& filter,
                             std::vector<std::string>* entries,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix) {
  std::map<std::string, std::string> keys_entries;
  bool result = LoadKeysAndEntriesWithFilter(filter, &keys_entries, options,
                                             target_prefix);
  if (!result)
    return false;

  for (const auto& pair : keys_entries)
    entries->push_back(pair.second);
  return true;
}

bool LevelDB::LoadKeysAndEntries(
    std::map<std::string, std::string>* keys_entries) {
  return LoadKeysAndEntriesWithFilter(KeyFilter(), keys_entries);
}

bool LevelDB::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    std::map<std::string, std::string>* keys_entries) {
  return LoadKeysAndEntriesWithFilter(filter, keys_entries,
                                      leveldb::ReadOptions(), std::string());
}

bool LevelDB::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    std::map<std::string, std::string>* keys_entries,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix) {
  return LoadKeysAndEntriesWhile(
      filter, keys_entries, options, target_prefix,
      base::BindRepeating(&PrefixStopCallback, target_prefix));
}

bool LevelDB::LoadKeysAndEntriesWhile(
    const KeyFilter& filter,
    std::map<std::string, std::string>* keys_entries,
    const leveldb::ReadOptions& options,
    const std::string& start_key,
    const KeyFilter& while_callback) {
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  std::unique_ptr<leveldb::Iterator> db_iterator(db_->NewIterator(options));
  leveldb::Slice start(start_key);
  for (db_iterator->Seek(start);
       db_iterator->Valid() &&
       while_callback.Run(db_iterator->key().ToString());
       db_iterator->Next()) {
    leveldb::Slice key_slice = db_iterator->key();
    std::string key_slice_str(key_slice.data(), key_slice.size());
    if (!filter.is_null() && !filter.Run(key_slice_str)) {
      continue;
    }

    leveldb::Slice value_slice = db_iterator->value();
    keys_entries->insert(std::make_pair(
        key_slice_str, std::string(value_slice.data(), value_slice.size())));
  }
  return true;
}

bool LevelDB::LoadKeys(std::vector<std::string>* keys) {
  return LoadKeys(std::string(), keys);
}

bool LevelDB::LoadKeys(const std::string& target_prefix,
                       std::vector<std::string>* keys) {
  leveldb::ReadOptions options;
  options.fill_cache = false;
  std::map<std::string, std::string> keys_entries;
  bool result = LoadKeysAndEntriesWithFilter(KeyFilter(), &keys_entries,
                                             options, target_prefix);
  if (!result)
    return false;

  for (const auto& pair : keys_entries)
    keys->push_back(pair.first);
  return true;
}

bool LevelDB::Get(const std::string& key,
                  bool* found,
                  std::string* entry,
                  leveldb::Status* status) {
  DCHECK(status);
  DFAKE_SCOPED_LOCK(thread_checker_);
  if (!db_)
    return false;

  leveldb::ReadOptions options;
  *status = db_->Get(options, key, entry);
  if (status->ok()) {
    *found = true;
    return true;
  }
  if (status->IsNotFound()) {
    *found = false;
    return true;
  }

  DLOG(WARNING) << "Failed loading leveldb_proto entry with key \"" << key
                << "\": " << status->ToString();
  return false;
}

leveldb::Status LevelDB::Destroy() {
  db_.reset();
  const std::string path = database_dir_.AsUTF8Unsafe();
  leveldb::Status status = leveldb::DestroyDB(path, open_options_);
  if (!status.ok())
    LOG(WARNING) << "Unable to destroy " << path << ": " << status.ToString();
  return status;
}

bool LevelDB::GetApproximateMemoryUse(uint64_t* approx_mem) {
  std::string usage_string;
  return (db_->GetProperty("leveldb.approximate-memory-usage", &usage_string) &&
          base::StringToUint64(usage_string, approx_mem));
}

}  // namespace leveldb_proto
