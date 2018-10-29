// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_LEVELDB_WRAPPER_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_LEVELDB_WRAPPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}

namespace leveldb_proto {

using KeyValueVector = base::StringPairs;
using KeyVector = std::vector<std::string>;

// When the ProtoDatabase instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
// Construction/calls/destruction should all happen on the same thread.
class ProtoLevelDBWrapper {
 public:
  using InitCallback = base::OnceCallback<void(bool success)>;
  using UpdateCallback = base::OnceCallback<void(bool success)>;
  using LoadKeysCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::vector<std::string>>)>;
  using DestroyCallback = base::OnceCallback<void(bool success)>;
  using OnCreateCallback = base::OnceCallback<void(ProtoLevelDBWrapper*)>;

  template <typename T>
  class Internal {
   public:
    using LoadCallback =
        base::OnceCallback<void(bool success, std::unique_ptr<std::vector<T>>)>;
    using GetCallback =
        base::OnceCallback<void(bool success, std::unique_ptr<T>)>;
    using LoadKeysAndEntriesCallback =
        base::OnceCallback<void(bool success,
                                std::unique_ptr<std::map<std::string, T>>)>;

    // A list of key-value (string, T) tuples.
    using KeyEntryVector = std::vector<std::pair<std::string, T>>;
  };

  // All blocking calls/disk access will happen on the provided |task_runner|.
  ProtoLevelDBWrapper(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  virtual ~ProtoLevelDBWrapper();

  template <typename T>
  void UpdateEntries(
      std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<KeyVector> keys_to_remove,
      UpdateCallback callback);

  template <typename T>
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      UpdateCallback callback);

  template <typename T>
  void LoadEntries(
      typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& key_filter,
      typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback);

  template <typename T>
  void LoadKeysAndEntries(
      typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
          callback);

  template <typename T>
  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
          callback);

  template <typename T>
  void LoadKeysAndEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
          callback);

  void LoadKeys(LoadKeysCallback callback);

  template <typename T>
  void GetEntry(
      const std::string& key,
      typename ProtoLevelDBWrapper::Internal<T>::GetCallback callback);

  void Destroy(DestroyCallback callback);

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        InitCallback callback);

  bool GetApproximateMemoryUse(uint64_t* approx_mem_use);
  const scoped_refptr<base::SequencedTaskRunner>& task_runner();

 private:
  THREAD_CHECKER(thread_checker_);

  // Used to run blocking tasks in-order, must be the TaskRunner that |db_|
  // relies on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  LevelDB* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ProtoLevelDBWrapper);
};

namespace {

template <typename T>
void RunUpdateCallback(typename ProtoLevelDBWrapper::UpdateCallback callback,
                       const bool* success) {
  std::move(callback).Run(*success);
}

template <typename T>
void RunLoadCallback(
    typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback,
    bool* success,
    std::unique_ptr<std::vector<T>> entries) {
  std::move(callback).Run(*success, std::move(entries));
}

template <typename T>
void RunLoadKeysAndEntriesCallback(
    typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
        callback,
    bool* success,
    std::unique_ptr<std::map<std::string, T>> keys_entries) {
  std::move(callback).Run(*success, std::move(keys_entries));
}

template <typename T>
void RunGetCallback(
    typename ProtoLevelDBWrapper::Internal<T>::GetCallback callback,
    const bool* success,
    const bool* found,
    std::unique_ptr<T> entry) {
  std::move(callback).Run(*success, *found ? std::move(entry) : nullptr);
}

template <typename T>
void UpdateEntriesFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
        entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    bool* success) {
  DCHECK(success);

  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (const auto& pair : *entries_to_save) {
    pairs_to_save.push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  *success = database->Save(pairs_to_save, *keys_to_remove);
}

template <typename T>
void UpdateEntriesWithRemoveFilterFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
        entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    bool* success) {
  DCHECK(success);

  // Serialize the values from Proto to string before passing on to database.
  KeyValueVector pairs_to_save;
  for (const auto& pair : *entries_to_save) {
    pairs_to_save.push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  *success = database->UpdateWithRemoveFilter(pairs_to_save, delete_key_filter);
}

template <typename T>
void LoadKeysAndEntriesFromTaskRunner(LevelDB* database,
                                      const LevelDB::KeyFilter& filter,
                                      const leveldb::ReadOptions& options,
                                      const std::string& target_prefix,
                                      std::map<std::string, T>* keys_entries,
                                      bool* success) {
  DCHECK(success);
  DCHECK(keys_entries);

  keys_entries->clear();

  std::map<std::string, std::string> loaded_entries;
  *success = database->LoadKeysAndEntriesWithFilter(filter, &loaded_entries,
                                                    options, target_prefix);

  for (const auto& pair : loaded_entries) {
    T entry;
    if (!entry.ParseFromString(pair.second)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry";
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }

    keys_entries->insert(std::make_pair(pair.first, entry));
  }
}

template <typename T>
void LoadEntriesFromTaskRunner(LevelDB* database,
                               const LevelDB::KeyFilter& filter,
                               const leveldb::ReadOptions& options,
                               const std::string& target_prefix,
                               std::vector<T>* entries,
                               bool* success) {
  DCHECK(success);
  DCHECK(entries);

  entries->clear();

  std::vector<std::string> loaded_entries;
  *success =
      database->LoadWithFilter(filter, &loaded_entries, options, target_prefix);

  for (const auto& serialized_entry : loaded_entries) {
    T entry;
    if (!entry.ParseFromString(serialized_entry)) {
      DLOG(WARNING) << "Unable to parse leveldb_proto entry";
      // TODO(cjhopman): Decide what to do about un-parseable entries.
    }

    entries->push_back(entry);
  }
}

template <typename T>
void GetEntryFromTaskRunner(LevelDB* database,
                            const std::string& key,
                            T* entry,
                            bool* found,
                            bool* success) {
  DCHECK(success);
  DCHECK(found);
  DCHECK(entry);

  std::string serialized_entry;
  *success = database->Get(key, found, &serialized_entry);

  if (!*success) {
    *found = false;
    return;
  }

  if (!*found)
    return;

  if (!entry->ParseFromString(serialized_entry)) {
    *found = false;
    DLOG(WARNING) << "Unable to parse leveldb_proto entry";
    // TODO(cjhopman): Decide what to do about un-parseable entries.
  }
}

}  // namespace

template <typename T>
void ProtoLevelDBWrapper::UpdateEntries(
    std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
        entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    typename ProtoLevelDBWrapper::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(UpdateEntriesFromTaskRunner<T>, base::Unretained(db_),
                     std::move(entries_to_save), std::move(keys_to_remove),
                     success),
      base::BindOnce(RunUpdateCallback<T>, std::move(callback),
                     base::Owned(success)));
}

template <typename T>
void ProtoLevelDBWrapper::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoLevelDBWrapper::Internal<T>::KeyEntryVector>
        entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    typename ProtoLevelDBWrapper::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool* success = new bool(false);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(UpdateEntriesWithRemoveFilterFromTaskRunner<T>,
                     base::Unretained(db_), std::move(entries_to_save),
                     delete_key_filter, success),
      base::BindOnce(RunUpdateCallback<T>, std::move(callback),
                     base::Owned(success)));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntries(
    typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter<T>(LevelDB::KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter<T>(key_filter, leveldb::ReadOptions(), std::string(),
                           std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename ProtoLevelDBWrapper::Internal<T>::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool* success = new bool(false);

  std::unique_ptr<std::vector<T>> entries(new std::vector<T>());
  // Get this pointer before entries is std::move()'d so we can use it below.
  std::vector<T>* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadEntriesFromTaskRunner<T>, base::Unretained(db_),
                     key_filter, options, target_prefix, entries_ptr, success),
      base::BindOnce(RunLoadCallback<T>, std::move(callback),
                     base::Owned(success), std::move(entries)));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntries(
    typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
        callback) {
  LoadKeysAndEntriesWithFilter<T>(LevelDB::KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
        callback) {
  LoadKeysAndEntriesWithFilter<T>(key_filter, leveldb::ReadOptions(),
                                  std::string(), std::move(callback));
}

template <typename T>
void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename ProtoLevelDBWrapper::Internal<T>::LoadKeysAndEntriesCallback
        callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool* success = new bool(false);

  auto keys_entries = std::make_unique<std::map<std::string, T>>();
  // Get this pointer before entries is std::move()'d so we can use it below.
  std::map<std::string, T>* keys_entries_ptr = keys_entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadKeysAndEntriesFromTaskRunner<T>, base::Unretained(db_),
                     key_filter, options, target_prefix, keys_entries_ptr,
                     success),
      base::BindOnce(RunLoadKeysAndEntriesCallback<T>, std::move(callback),
                     base::Owned(success), std::move(keys_entries)));
}

template <typename T>
void ProtoLevelDBWrapper::GetEntry(
    const std::string& key,
    typename ProtoLevelDBWrapper::Internal<T>::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool* success = new bool(false);
  bool* found = new bool(false);

  std::unique_ptr<T> entry(new T());
  // Get this pointer before entry is std::move()'d so we can use it below.
  T* entry_ptr = entry.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(GetEntryFromTaskRunner<T>, base::Unretained(db_), key,
                     entry_ptr, found, success),
      base::BindOnce(RunGetCallback<T>, std::move(callback),
                     base::Owned(success), base::Owned(found),
                     std::move(entry)));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_LEVELDB_WRAPPER_H_