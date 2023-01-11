// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PERSISTENT_KEY_VALUE_STORE_IMPL_H_
#define COMPONENTS_FEED_CORE_V2_PERSISTENT_KEY_VALUE_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/offline_pages/task/task_queue.h"

namespace feedkvstore {
class Entry;
}
namespace feed {
namespace internal {
constexpr int kMaxEntriesInMemory = 50;
}  // namespace internal

// A generic persistent key-value cache. Has a maximum size determined by
// `feed::Config`. Once size of all values exceed the maximum, older keys
// are eventually evicted. Key age is determined only by the last call to
// `Put()`.
class PersistentKeyValueStoreImpl : public PersistentKeyValueStore {
 public:
  using Result = PersistentKeyValueStore::Result;
  using ResultCallback = base::OnceCallback<void(Result)>;

  explicit PersistentKeyValueStoreImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedkvstore::Entry>>
          database);
  ~PersistentKeyValueStoreImpl() override;
  PersistentKeyValueStoreImpl(const PersistentKeyValueStoreImpl&) = delete;
  PersistentKeyValueStoreImpl& operator=(const PersistentKeyValueStoreImpl&) =
      delete;

  // PersistentKeyValueStore methods.

  // Erase all data in the store.
  void ClearAll(ResultCallback callback) override;
  // Write/overwrite a key/value pair.
  void Put(const std::string& key,
           const std::string& value,
           ResultCallback callback) override;
  // Get a value by key.
  void Get(const std::string& key, ResultCallback callback) override;
  // Delete a value by key.
  void Delete(const std::string& key, ResultCallback callback) override;

  // Evict old stored entries until total size of all values in the database
  // is less than max_db_size_bytes.
  void EvictOldEntries(ResultCallback callback);

  leveldb_proto::ProtoDatabase<feedkvstore::Entry>* GetDatabase() {
    return database_.get();
  }

  base::WeakPtr<PersistentKeyValueStoreImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool IsTaskRunningForTesting() const { return running_task_; }

 private:
  enum class TaskType { kGet, kPut, kDelete, kClearAll, kEvictOldEntries };
  // Represents any operation on the database. Allows us to easily perform lazy
  // initialization, and serialize db operations.
  struct Task {
    Task();
    Task(TaskType type, ResultCallback callback);
    Task(TaskType type, std::string key, ResultCallback callback);
    Task(Task&&) noexcept;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task();

    TaskType type;
    // Key for kGet, kPut, and kDelete.
    std::string key;
    // Value for kPut.
    std::string put_value;
    ResultCallback done_callback;
  };

  void AddTask(Task task);
  // Implementation functions for potentially queueable actions.
  void StartTask(Task task);

  void GetDone(Task task, bool ok, std::unique_ptr<feedkvstore::Entry> entry);
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  void TaskComplete(Task task, Result result);
  void TaskCompleteBool(Task task, bool ok);

  bool IsInitialized() const;

  bool running_task_ = false;
  base::queue<Task> queued_tasks_;
  bool triggered_initialize_ = false;
  leveldb_proto::Enums::InitStatus database_status_ =
      leveldb_proto::Enums::InitStatus::kNotInitialized;
  std::unique_ptr<leveldb_proto::ProtoDatabase<feedkvstore::Entry>> database_;
  base::WeakPtrFactory<PersistentKeyValueStoreImpl> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PERSISTENT_KEY_VALUE_STORE_IMPL_H_
