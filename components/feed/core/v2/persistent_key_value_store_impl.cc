// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/persistent_key_value_store_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/offline_pages/task/task.h"

namespace feed {
namespace {
using ::feed::internal::kMaxEntriesInMemory;
using feedkvstore::Entry;
}  // namespace

// Eviction task functionality.
class EvictTask {
 public:
  static void Start(base::WeakPtr<PersistentKeyValueStoreImpl> store,
                    base::OnceCallback<void(bool)> done_callback) {
    auto state = std::make_unique<State>();
    state->store = store;
    state->done_callback = std::move(done_callback);

    auto* db = GetDbOrFinish(state);
    if (!db)
      return;
    db->LoadKeys(base::BindOnce(&EvictTask::LoadKeysDone, std::move(state)));
  }

 private:
  struct EntryMetadata {
    std::string key;
    int64_t size_bytes;
    int64_t modification_time;
  };

  struct State {
    base::WeakPtr<PersistentKeyValueStoreImpl> store;
    base::OnceCallback<void(bool)> done_callback;

    std::vector<std::string> all_keys;
    size_t next_key_index = 0;
    std::vector<EntryMetadata> metadata;
  };

  static void Finish(std::unique_ptr<State> state) {
    std::move(state->done_callback).Run(true);
  }

  static leveldb_proto::ProtoDatabase<Entry>* GetDbOrFinish(
      std::unique_ptr<State>& state) {
    if (state->store) {
      return state->store->GetDatabase();
    }
    Finish(std::move(state));
    return nullptr;
  }

  static void IndexMore(std::unique_ptr<State> state) {
    auto* db = GetDbOrFinish(state);
    if (!db)
      return;
    if (state->next_key_index >= state->all_keys.size()) {
      IndexingDone(std::move(state));
      return;
    }
    const size_t first_index = state->next_key_index;
    std::string last_key;
    state->next_key_index = std::min(
        state->next_key_index + kMaxEntriesInMemory, state->all_keys.size());

    std::string lower_bound = state->all_keys[first_index],
                upper_bound = state->all_keys[state->next_key_index - 1];
    db->LoadKeysAndEntriesInRange(
        lower_bound, upper_bound,
        base::BindOnce(&EvictTask::IndexMore_LoadChunkDone, std::move(state)));
  }

  static void IndexMore_LoadChunkDone(
      std::unique_ptr<State> state,
      bool ok,
      std::unique_ptr<std::map<std::string, feedkvstore::Entry>> entries) {
    const int64_t now =
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
    for (auto& entry : *entries) {
      EntryMetadata m;
      m.key = entry.first;
      m.modification_time = entry.second.modification_time();
      // If modification time is in the future, assume the information is out
      // of date.
      if (m.modification_time > now)
        m.modification_time = 0;
      m.size_bytes = entry.second.value().size();
      state->metadata.push_back(m);
    }
    IndexMore(std::move(state));
  }

  static void LoadKeysDone(std::unique_ptr<State> state,
                           bool ok,
                           std::unique_ptr<std::vector<std::string>> keys) {
    if (!ok || !keys) {
      Finish(std::move(state));
      return;
    }
    state->all_keys = std::move(*keys);
    IndexMore(std::move(state));
  }

  static void IndexingDone(std::unique_ptr<State> state) {
    auto* db = GetDbOrFinish(state);
    if (!db)
      return;
    std::sort(state->metadata.begin(), state->metadata.end(),
              [&](const EntryMetadata& a, const EntryMetadata& b) {
                return a.modification_time > b.modification_time;
              });

    size_t i = 0;
    int64_t total_size = 0;
    const int64_t max_db_size_bytes =
        GetFeedConfig().persistent_kv_store_maximum_size_before_eviction;
    for (; i < state->metadata.size(); ++i) {
      total_size += state->metadata[i].size_bytes;
      if (total_size > max_db_size_bytes) {
        break;
      }
    }

    auto keys_to_remove = std::make_unique<std::vector<std::string>>();
    for (; i < state->metadata.size(); ++i) {
      keys_to_remove->push_back(state->metadata[i].key);
    }

    db->UpdateEntries(
        std::make_unique<std::vector<std::pair<std::string, Entry>>>(),
        std::move(keys_to_remove),
        base::BindOnce([](std::unique_ptr<State> state,
                          bool ok) { Finish(std::move(state)); },
                       std::move(state)));
  }
};

PersistentKeyValueStoreImpl::Task::Task() = default;
PersistentKeyValueStoreImpl::Task::Task(TaskType task_type,
                                        ResultCallback callback)
    : Task(task_type, std::string(), std::move(callback)) {}
PersistentKeyValueStoreImpl::Task::Task(TaskType task_type,
                                        std::string key,
                                        ResultCallback callback)
    : type(task_type), key(key), done_callback(std::move(callback)) {}
PersistentKeyValueStoreImpl::Task::Task(Task&&) noexcept = default;
PersistentKeyValueStoreImpl::Task::~Task() = default;

PersistentKeyValueStoreImpl::PersistentKeyValueStoreImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<feedkvstore::Entry>> database)
    : database_(std::move(database)) {}

PersistentKeyValueStoreImpl::~PersistentKeyValueStoreImpl() = default;

void PersistentKeyValueStoreImpl::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  database_status_ = status;
  TaskComplete({}, {});
}

bool PersistentKeyValueStoreImpl::IsInitialized() const {
  return database_status_ == leveldb_proto::Enums::InitStatus::kOK;
}

void PersistentKeyValueStoreImpl::AddTask(Task task) {
  if (!triggered_initialize_) {
    triggered_initialize_ = true;
    running_task_ = true;
    database_->Init(base::BindOnce(
        &PersistentKeyValueStoreImpl::OnDatabaseInitialized, GetWeakPtr()));
  }
  if (!running_task_) {
    StartTask(std::move(task));
  } else {
    queued_tasks_.push(std::move(task));
  }
}

void PersistentKeyValueStoreImpl::ClearAll(ResultCallback callback) {
  AddTask({TaskType::kClearAll, std::move(callback)});
}

void PersistentKeyValueStoreImpl::Put(const std::string& key,
                                      const std::string& value,
                                      ResultCallback callback) {
  // Use a random number to trigger EvictOldEntries().
  // The expected number of calls to EvictOldEntries() is =~
  // (sum of bytes written) / `cleanup_interval_in_written_bytes`.
  int cleanup_interval_in_written_bytes =
      GetFeedConfig().persistent_kv_store_cleanup_interval_in_written_bytes;
  int rand_int = base::RandInt(0, cleanup_interval_in_written_bytes);
  if (cleanup_interval_in_written_bytes > 0 &&
      rand_int < static_cast<int>(value.size())) {
    EvictOldEntries(base::DoNothing());
  }
  Task task(TaskType::kPut, key, std::move(callback));
  task.put_value = value;
  AddTask(std::move(task));
}

void PersistentKeyValueStoreImpl::Get(const std::string& key,
                                      ResultCallback callback) {
  AddTask({TaskType::kGet, key, std::move(callback)});
}

void PersistentKeyValueStoreImpl::Delete(const std::string& key,
                                         ResultCallback callback) {
  AddTask({TaskType::kDelete, key, std::move(callback)});
}

void PersistentKeyValueStoreImpl::EvictOldEntries(ResultCallback callback) {
  AddTask({TaskType::kEvictOldEntries, std::move(callback)});
}

void PersistentKeyValueStoreImpl::StartTask(Task task) {
  if (!IsInitialized()) {
    TaskComplete(std::move(task), {});
    return;
  }
  running_task_ = true;

  switch (task.type) {
    case TaskType::kGet: {
      std::string key = std::move(task.key);
      database_->GetEntry(key,
                          base::BindOnce(&PersistentKeyValueStoreImpl::GetDone,
                                         GetWeakPtr(), std::move(task)));
      break;
    }
    case TaskType::kPut: {
      auto entries_to_save = std::make_unique<
          leveldb_proto::ProtoDatabase<feedkvstore::Entry>::KeyEntryVector>();
      {
        feedkvstore::Entry new_entry;
        new_entry.set_value(std::move(task.put_value));
        new_entry.set_modification_time(
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds());
        entries_to_save->emplace_back(task.key, std::move(new_entry));
      }
      database_->UpdateEntries(
          std::move(entries_to_save),
          /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
          base::BindOnce(&PersistentKeyValueStoreImpl::TaskCompleteBool,
                         GetWeakPtr(), std::move(task)));
      break;
    }
    case TaskType::kDelete: {
      auto keys_to_remove = std::make_unique<std::vector<std::string>>();
      keys_to_remove->push_back(task.key);
      database_->UpdateEntries(
          std::make_unique<std::vector<std::pair<std::string, Entry>>>(),
          std::move(keys_to_remove),
          base::BindOnce(&PersistentKeyValueStoreImpl::TaskCompleteBool,
                         GetWeakPtr(), std::move(task)));
      break;
    }
    case TaskType::kClearAll: {
      auto filter = [](const std::string& key) { return true; };
      database_->UpdateEntriesWithRemoveFilter(
          std::make_unique<
              std::vector<std::pair<std::string, feedkvstore::Entry>>>(),
          base::BindRepeating(filter),
          base::BindOnce(&PersistentKeyValueStoreImpl::TaskCompleteBool,
                         GetWeakPtr(), std::move(task)));
      break;
    }
    case TaskType::kEvictOldEntries: {
      EvictTask::Start(
          GetWeakPtr(),
          base::BindOnce(&PersistentKeyValueStoreImpl::TaskCompleteBool,
                         GetWeakPtr(), std::move(task)));
      break;
    }
  }
}

void PersistentKeyValueStoreImpl::GetDone(
    Task task,
    bool ok,
    std::unique_ptr<feedkvstore::Entry> get_entry) {
  Result result;
  if (ok && get_entry) {
    result.success = true;
    result.get_result = std::move(get_entry->value());
  } else {
    result.success = ok;
  }
  TaskComplete(std::move(task), std::move(result));
}

void PersistentKeyValueStoreImpl::TaskComplete(Task complete_task,
                                               Result result) {
  if (complete_task.done_callback) {
    std::move(complete_task.done_callback).Run(std::move(result));
  }
  if (queued_tasks_.empty()) {
    running_task_ = false;
    return;
  }
  Task new_task = std::move(queued_tasks_.front());
  queued_tasks_.pop();
  StartTask(std::move(new_task));
}

void PersistentKeyValueStoreImpl::TaskCompleteBool(Task complete_task,
                                                   bool ok) {
  Result result;
  result.success = ok;
  return TaskComplete(std::move(complete_task), std::move(result));
}

}  // namespace feed
