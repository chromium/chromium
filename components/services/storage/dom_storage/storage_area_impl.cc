// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/storage_area_impl.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace storage {

BASE_FEATURE(kDomStorageSmartFlushing,
             "DomStorageSmartFlushing",
             base::FEATURE_DISABLED_BY_DEFAULT);

StorageAreaImpl::Delegate::~Delegate() = default;

void StorageAreaImpl::Delegate::PrepareToCommit(
    std::vector<DomStorageDatabase::KeyValuePair>* extra_entries_to_add,
    std::vector<DomStorageDatabase::Key>* extra_keys_to_delete) {}

void StorageAreaImpl::Delegate::OnMapLoaded(leveldb::Status) {}

bool StorageAreaImpl::s_aggressive_flushing_enabled_ = false;

StorageAreaImpl::RateLimiter::RateLimiter(size_t desired_rate,
                                          base::TimeDelta time_quantum)
    : rate_(desired_rate), samples_(0), time_quantum_(time_quantum) {
  DCHECK_GT(desired_rate, 0ul);
}

base::TimeDelta StorageAreaImpl::RateLimiter::ComputeTimeNeeded() const {
  return time_quantum_ * (samples_ / rate_);
}

base::TimeDelta StorageAreaImpl::RateLimiter::ComputeDelayNeeded(
    const base::TimeDelta elapsed_time) const {
  base::TimeDelta time_needed = ComputeTimeNeeded();
  if (time_needed > elapsed_time)
    return time_needed - elapsed_time;
  return base::TimeDelta();
}

StorageAreaImpl::CommitBatch::CommitBatch() = default;

StorageAreaImpl::CommitBatch::~CommitBatch() = default;

StorageAreaImpl::StorageAreaImpl(AsyncDomStorageDatabase* database,
                                 const std::string& prefix,
                                 Delegate* delegate,
                                 const Options& options)
    : StorageAreaImpl(database,
                      std::vector<uint8_t>(prefix.begin(), prefix.end()),
                      delegate,
                      options) {}

StorageAreaImpl::StorageAreaImpl(AsyncDomStorageDatabase* database,
                                 std::vector<uint8_t> prefix,
                                 Delegate* delegate,
                                 const Options& options)
    : prefix_(std::move(prefix)),
      delegate_(delegate),
      database_(database),
      cache_mode_(database ? options.cache_mode : CacheMode::KEYS_AND_VALUES),
      storage_used_(0),
      max_size_(options.max_size),
      memory_used_(0),
      start_time_(base::TimeTicks::Now()),
      default_commit_delay_(options.default_commit_delay),
      data_rate_limiter_(options.max_bytes_per_hour, base::Hours(1)),
      commit_rate_limiter_(options.max_commits_per_hour, base::Hours(1)) {
  if (database_) {
    database_->AddCommitter(this);
  }
  receivers_.set_disconnect_handler(base::BindRepeating(
      &StorageAreaImpl::OnConnectionError, weak_ptr_factory_.GetWeakPtr()));
}

StorageAreaImpl::~StorageAreaImpl() {
  DCHECK(!has_pending_load_tasks());
  if (commit_batch_)
    CommitChanges();
  if (database_) {
    database_->RemoveCommitter(this);
  }
}

void StorageAreaImpl::InitializeAsEmpty() {
  DCHECK_EQ(map_state_, MapState::UNLOADED);
  map_state_ = MapState::LOADING_FROM_DATABASE;
  OnMapLoaded(leveldb::Status::OK(), {});
}

void StorageAreaImpl::Bind(
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  receivers_.Add(this, std::move(receiver));
  // If the number of bindings is more than 1, then the |client_old_value| sent
  // by the clients need not be valid due to races on updates from multiple
  // clients. So, cache the values in the service. Setting cache mode back to
  // only keys when the number of bindings goes back to 1 could cause
  // inconsistency due to the async notifications of mutations to the client
  // reaching late.
  if (cache_mode_ == CacheMode::KEYS_ONLY_WHEN_POSSIBLE &&
      receivers_.size() > 1) {
    SetCacheMode(CacheMode::KEYS_AND_VALUES);
  }
}

std::unique_ptr<StorageAreaImpl> StorageAreaImpl::ForkToNewPrefix(
    const std::string& new_prefix,
    Delegate* delegate,
    const Options& options) {
  return ForkToNewPrefix(
      std::vector<uint8_t>(new_prefix.begin(), new_prefix.end()), delegate,
      options);
}

std::unique_ptr<StorageAreaImpl> StorageAreaImpl::ForkToNewPrefix(
    std::vector<uint8_t> new_prefix,
    Delegate* delegate,
    const Options& options) {
  auto forked_area = std::make_unique<StorageAreaImpl>(
      database_, std::move(new_prefix), delegate, options);
  // If the source map is empty, don't bother hitting disk.
  if (IsMapLoadedAndEmpty()) {
    forked_area->InitializeAsEmpty();
    return forked_area;
  }
  forked_area->map_state_ = MapState::LOADING_FROM_FORK;

  if (IsMapLoaded()) {
    DoForkOperation(forked_area->weak_ptr_factory_.GetWeakPtr());
  } else {
    LoadMap(base::BindOnce(&StorageAreaImpl::DoForkOperation,
                           weak_ptr_factory_.GetWeakPtr(),
                           forked_area->weak_ptr_factory_.GetWeakPtr()));
  }
  return forked_area;
}

void StorageAreaImpl::CancelAllPendingRequests() {
  on_load_complete_tasks_.clear();
}

void StorageAreaImpl::EnableAggressiveCommitDelay() {
  s_aggressive_flushing_enabled_ = true;
}

void StorageAreaImpl::ScheduleImmediateCommit() {
  if (!on_load_complete_tasks_.empty()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::ScheduleImmediateCommit,
                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!database_ || !commit_batch_) {
    return;
  }
  CommitChanges();
}

void StorageAreaImpl::OnMemoryDump(const std::string& name,
                                   base::trace_event::ProcessMemoryDump* pmd) {
  if (!IsMapLoaded())
    return;

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (commit_batch_) {
    size_t data_size = 0;
    for (const auto& iter : commit_batch_->changed_values)
      data_size += iter.first.size() + iter.second.size();
    for (const auto& key : commit_batch_->changed_keys)
      data_size += key.size();

    auto* commit_batch_mad = pmd->CreateAllocatorDump(name + "/commit_batch");
    commit_batch_mad->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, data_size);
    if (system_allocator_name)
      pmd->AddSuballocation(commit_batch_mad->guid(), system_allocator_name);
  }

  // Do not add storage map usage if less than 1KB.
  if (memory_used_ < 1024)
    return;

  auto* map_mad = pmd->CreateAllocatorDump(name + "/storage_map");
  map_mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     memory_used_);
  map_mad->AddString("load_state", "",
                     map_state_ == MapState::LOADED_KEYS_ONLY
                         ? "keys_only"
                         : "keys_and_values");
  if (system_allocator_name)
    pmd->AddSuballocation(map_mad->guid(), system_allocator_name);
}

void StorageAreaImpl::PurgeMemory() {
  if (!IsMapLoaded() ||  // We're not using any memory.
      commit_batch_ ||   // We leave things alone with changes pending.
      !database_) {  // Don't purge anything if we're not backed by a database.
    return;
  }

  map_state_ = MapState::UNLOADED;
  memory_used_ = 0;
  keys_only_map_.clear();
  keys_values_map_.clear();
}

void StorageAreaImpl::SetCacheModeForTesting(CacheMode cache_mode) {
  SetCacheMode(cache_mode);
}

void StorageAreaImpl::AddObserver(
    mojo::PendingRemote<blink::mojom::StorageAreaObserver> observer) {
  mojo::Remote<blink::mojom::StorageAreaObserver> observer_remote(
      std::move(observer));
  if (cache_mode_ == CacheMode::KEYS_AND_VALUES)
    observer_remote->ShouldSendOldValueOnMutations(false);
  observers_.Add(std::move(observer_remote));
}

void StorageAreaImpl::Put(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& value,
    const std::optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    PutCallback callback) {
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Put,
                           weak_ptr_factory_.GetWeakPtr(), key, value,
                           client_old_value, source, std::move(callback)));
    return;
  }

  size_t old_item_size = 0;
  size_t old_item_memory = 0;
  size_t new_item_memory = 0;
  std::optional<std::vector<uint8_t>> old_value;
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    KeysOnlyMap::const_iterator found = keys_only_map_.find(key);
    if (found != keys_only_map_.end()) {
      if (client_old_value &&
          client_old_value.value().size() == found->second) {
        if (client_old_value == value) {
          // NOTE: Even though the key is not changing, we have to acknowledge
          // the change request, as clients may rely on this acknowledgement for
          // caching behavior.
          for (const auto& observer : observers_)
            observer->KeyChanged(key, value, value, source);
          std::move(callback).Run(true);  // Key already has this value.
          return;
        }
        old_value = client_old_value.value();
      } else {
#if DCHECK_IS_ON()
        // If |client_old_value| was not provided or if it's size does not
        // match, then we still let the change go through. But the notification
        // sent to clients will not contain old value. This is okay since
        // currently the only observer to these notification is the client
        // itself.
        DVLOG(1) << "Storage area with prefix "
                 << std::string(prefix_.begin(), prefix_.end())
                 << ": past value has length of " << found->second << ", but:";
        if (client_old_value) {
          DVLOG(1) << "Given past value has incorrect length of "
                   << client_old_value.value().size();
        } else {
          DVLOG(1) << "No given past value was provided.";
        }
#endif
      }
      old_item_size = key.size() + found->second;
      old_item_memory = key.size() + sizeof(size_t);
    }
    new_item_memory = key.size() + sizeof(size_t);
  } else {
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_AND_VALUES);
    auto found = keys_values_map_.find(key);
    if (found != keys_values_map_.end()) {
      if (found->second == value) {
        // NOTE: Even though the key is not changing, we have to acknowledge
        // the change request, as clients may rely on this acknowledgement for
        // caching behavior.
        for (const auto& observer : observers_)
          observer->KeyChanged(key, value, value, source);
        std::move(callback).Run(true);  // Key already has this value.
        return;
      }
      old_value = std::move(found->second);
      old_item_size = key.size() + old_value.value().size();
      old_item_memory = old_item_size;
    }
    new_item_memory = key.size() + value.size();
  }

  size_t new_item_size = key.size() + value.size();
  size_t new_storage_used = storage_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing maps that are over budget.
  if (new_item_size > old_item_size && new_storage_used > max_size_) {
    if (map_state_ == MapState::LOADED_KEYS_ONLY) {
      receivers_.ReportBadMessage(
          "The quota in browser cannot exceed when there is only one "
          "renderer.");
    } else {
      for (const auto& observer : observers_)
        observer->KeyChangeFailed(key, source);
      std::move(callback).Run(false);
    }
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    // No need to store values in |commit_batch_| if values are already
    // available in |keys_values_map_|, since CommitChanges() will take values
    // from there.
    if (map_state_ == MapState::LOADED_KEYS_ONLY)
      commit_batch_->changed_values[key] = value;
    else
      commit_batch_->changed_keys.insert(key);

    commit_batch_->put_timestamps.push_back(base::TimeTicks::Now());
  }

  if (map_state_ == MapState::LOADED_KEYS_ONLY)
    keys_only_map_[key] = value.size();
  else
    keys_values_map_[key] = value;

  storage_used_ = new_storage_used;
  memory_used_ += new_item_memory - old_item_memory;
  for (const auto& observer : observers_)
    observer->KeyChanged(key, value, old_value, source);
  std::move(callback).Run(true);
}

void StorageAreaImpl::Delete(
    const std::vector<uint8_t>& key,
    const std::optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    DeleteCallback callback) {
  // Map upgrade check is required because the cache state could be changed
  // due to multiple bindings, and when multiple bindings are involved the
  // |client_old_value| can race. Thus any changes require checking for an
  // upgrade.
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Delete,
                           weak_ptr_factory_.GetWeakPtr(), key,
                           client_old_value, source, std::move(callback)));
    return;
  }

  if (database_)
    CreateCommitBatchIfNeeded();

  std::vector<uint8_t> old_value;
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    KeysOnlyMap::const_iterator found = keys_only_map_.find(key);
    if (found == keys_only_map_.end()) {
      // NOTE: Even though the key is not changing, we have to acknowledge
      // the change request, as clients may rely on this acknowledgement for
      // caching behavior.
      for (const auto& observer : observers_)
        observer->KeyDeleted(key, std::nullopt, source);
      std::move(callback).Run(true);
      return;
    }
    if (client_old_value && client_old_value.value().size() == found->second) {
      old_value = client_old_value.value();
    } else {
#if DCHECK_IS_ON()
      // If |client_old_value| was not provided or if it's size does not match,
      // then we still let the change go through. But the notification sent to
      // clients will not contain old value. This is okay since currently the
      // only observer to these notification is the client itself.
      DVLOG(1) << "Storage area with prefix "
               << std::string(prefix_.begin(), prefix_.end())
               << ": past value has length of " << found->second << ", but:";
      if (client_old_value) {
        DVLOG(1) << "Given past value has incorrect length of "
                 << client_old_value.value().size();
      } else {
        DVLOG(1) << "No given past value was provided.";
      }
#endif
    }
    storage_used_ -= key.size() + found->second;
    keys_only_map_.erase(found);
    memory_used_ -= key.size() + sizeof(size_t);
    if (commit_batch_)
      commit_batch_->changed_values[key] = std::vector<uint8_t>();
  } else {
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_AND_VALUES);
    auto found = keys_values_map_.find(key);
    if (found == keys_values_map_.end()) {
      // NOTE: Even though the key is not changing, we have to acknowledge
      // the change request, as clients may rely on this acknowledgement for
      // caching behavior.
      for (const auto& observer : observers_)
        observer->KeyDeleted(key, std::nullopt, source);
      std::move(callback).Run(true);
      return;
    }
    old_value.swap(found->second);
    keys_values_map_.erase(found);
    memory_used_ -= key.size() + old_value.size();
    storage_used_ -= key.size() + old_value.size();
    if (commit_batch_)
      commit_batch_->changed_keys.insert(key);
  }

  for (auto& observer : observers_)
    observer->KeyDeleted(key, old_value, source);
  std::move(callback).Run(true);
}

void StorageAreaImpl::DeleteAll(
    const std::string& source,
    mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
    DeleteAllCallback callback) {
  // Don't check if a map upgrade is needed here and instead just create an
  // empty map ourself.
  if (!IsMapLoaded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::DeleteAll,
                           weak_ptr_factory_.GetWeakPtr(), source,
                           std::move(new_observer), std::move(callback)));
    return;
  }

  bool already_empty = IsMapLoadedAndEmpty();

  // Upgrade map state if needed.
  if (IsMapUpgradeNeeded()) {
    DCHECK(keys_values_map_.empty());
    map_state_ = MapState::LOADED_KEYS_AND_VALUES;
  }

  if (new_observer)
    AddObserver(std::move(new_observer));

  if (already_empty) {
    for (const auto& observer : observers_)
      observer->AllDeleted(/*was_nonempty=*/false, source);
    std::move(callback).Run(true);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->clear_all_first = true;
    commit_batch_->changed_values.clear();
    commit_batch_->changed_keys.clear();
  }

  keys_only_map_.clear();
  keys_values_map_.clear();

  storage_used_ = 0;
  memory_used_ = 0;
  for (const auto& observer : observers_)
    observer->AllDeleted(/*was_nonempty=*/true, source);
  std::move(callback).Run(/*success=*/true);
}

void StorageAreaImpl::Get(const std::vector<uint8_t>& key,
                          GetCallback callback) {
  // TODO(ssid): Remove this method since it is not supported in only keys mode,
  // crbug.com/764127.
  if (cache_mode_ == CacheMode::KEYS_ONLY_WHEN_POSSIBLE) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Get,
                           weak_ptr_factory_.GetWeakPtr(), key,
                           std::move(callback)));
    return;
  }

  auto found = keys_values_map_.find(key);
  if (found == keys_values_map_.end()) {
    std::move(callback).Run(false, std::vector<uint8_t>());
    return;
  }
  std::move(callback).Run(true, found->second);
}

void StorageAreaImpl::GetAll(
    mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
    GetAllCallback callback) {
  // If the map is keys-only and empty, then no loading is necessary.
  if (IsMapLoadedAndEmpty()) {
    std::move(callback).Run(std::vector<blink::mojom::KeyValuePtr>());
    if (new_observer)
      AddObserver(std::move(new_observer));
    return;
  }

  // The map must always be loaded for the KEYS_ONLY_WHEN_POSSIBLE mode.
  if (map_state_ != MapState::LOADED_KEYS_AND_VALUES) {
    LoadMap(base::BindOnce(&StorageAreaImpl::GetAll,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(new_observer), std::move(callback)));
    return;
  }

  std::vector<blink::mojom::KeyValuePtr> all;
  for (const auto& it : keys_values_map_) {
    auto kv = blink::mojom::KeyValue::New();
    kv->key = it.first;
    kv->value = it.second;
    all.push_back(std::move(kv));
  }
  std::move(callback).Run(std::move(all));
  if (new_observer)
    AddObserver(std::move(new_observer));
}

base::OnceCallback<void(leveldb::Status)>
StorageAreaImpl::GetCommitCompleteCallback() {
  return base::BindOnce(&StorageAreaImpl::OnCommitComplete,
                        weak_ptr_factory_.GetWeakPtr());
}

void StorageAreaImpl::SetCacheMode(CacheMode cache_mode) {
  if (cache_mode_ == cache_mode ||
      (!database_ && cache_mode == CacheMode::KEYS_ONLY_WHEN_POSSIBLE)) {
    return;
  }
  cache_mode_ = cache_mode;
  bool should_send_values = cache_mode == CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
  for (auto& observer : observers_)
    observer->ShouldSendOldValueOnMutations(should_send_values);

  // If the |keys_only_map_| is loaded and desired state needs values, no point
  // keeping around the map since the next change would require reload. On the
  // other hand if only keys are desired, the keys and values map can still be
  // used. Consider not unloading when the map is still useful.
  UnloadMapIfPossible();
}

void StorageAreaImpl::Checkpoint() {
  if (!base::FeatureList::IsEnabled(kDomStorageSmartFlushing)) {
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  if (commit_rate_limiter_.ComputeDelayNeeded(elapsed_time).is_zero() &&
      data_rate_limiter_.ComputeDelayNeeded(elapsed_time).is_zero()) {
    ScheduleImmediateCommit();
  }
}

void StorageAreaImpl::OnConnectionError() {
  if (!receivers_.empty())
    return;
  // If any tasks are waiting for load to complete, delay calling the
  // no_bindings_callback_ until all those tasks have completed.
  if (!on_load_complete_tasks_.empty())
    return;
  delegate_->OnNoBindings();
}

void StorageAreaImpl::LoadMap(base::OnceClosure completion_callback) {
  DCHECK_NE(map_state_, MapState::LOADED_KEYS_AND_VALUES);
  DCHECK(keys_values_map_.empty());

  // Current commit batch needs to be applied before re-loading the map. The
  // re-load of map occurs only when GetAll() is called or CacheMode is set to
  // keys and values, and the |keys_only_map_| is already loaded. In this case
  // commit batch needs to be committed before reading the database.
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    DCHECK(on_load_complete_tasks_.empty());
    DCHECK(database_);
    if (commit_batch_)
      CommitChanges();
    // Make sure the keys only map is not used when on load tasks are in queue.
    // The changes to the area will be queued to on load tasks.
    keys_only_map_.clear();
    map_state_ = MapState::UNLOADED;
  }

  on_load_complete_tasks_.push_back(std::move(completion_callback));
  if (map_state_ == MapState::LOADING_FROM_DATABASE ||
      map_state_ == MapState::LOADING_FROM_FORK) {
    return;
  }

  map_state_ = MapState::LOADING_FROM_DATABASE;

  if (!database_) {
    OnMapLoaded(leveldb::Status::IOError(""), {});
    return;
  }

  database_->RunDatabaseTask(
      base::BindOnce(
          [](const DomStorageDatabase::Key& prefix,
             const DomStorageDatabase& db) {
            std::vector<DomStorageDatabase::KeyValuePair> data;
            leveldb::Status status = db.GetPrefixed(prefix, &data);
            return std::make_tuple(status, std::move(data));
          },
          prefix_),
      base::BindOnce(&StorageAreaImpl::OnMapLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StorageAreaImpl::OnMapLoaded(
    leveldb::Status status,
    std::vector<DomStorageDatabase::KeyValuePair> data) {
  DCHECK(keys_values_map_.empty());
  DCHECK_EQ(map_state_, MapState::LOADING_FROM_DATABASE);

  keys_only_map_.clear();
  map_state_ = MapState::LOADED_KEYS_AND_VALUES;

  keys_values_map_.clear();
  for (auto& entry : data) {
    DCHECK_GE(entry.key.size(), prefix_.size());
    keys_values_map_[DomStorageDatabase::Key(entry.key.begin() + prefix_.size(),
                                             entry.key.end())] =
        std::move(entry.value);
  }
  CalculateStorageAndMemoryUsed();

  // We proceed without using a backing store, nothing will be persisted but the
  // class is functional for the lifetime of the object.
  delegate_->OnMapLoaded(status);
  if (!status.ok()) {
    database_ = nullptr;
    SetCacheMode(CacheMode::KEYS_AND_VALUES);
  }

  if (on_load_callback_for_testing_)
    std::move(on_load_callback_for_testing_).Run();

  OnLoadComplete();
}

void StorageAreaImpl::CalculateStorageAndMemoryUsed() {
  memory_used_ = 0;
  storage_used_ = 0;

  for (auto& it : keys_values_map_)
    memory_used_ += it.first.size() + it.second.size();
  storage_used_ = memory_used_;

  for (auto& it : keys_only_map_) {
    memory_used_ += it.first.size() + sizeof(size_t);
    storage_used_ += it.first.size() + it.second;
  }
}

void StorageAreaImpl::OnLoadComplete() {
  DCHECK(IsMapLoaded());

  std::vector<base::OnceClosure> tasks;
  on_load_complete_tasks_.swap(tasks);
  for (auto it = tasks.begin(); it != tasks.end(); ++it) {
    // Some tasks (like GetAll) can require a reload if they need a different
    // map type. If this happens, stop our task execution. Appending tasks is
    // required (instead of replacing) because the task that required the
    // reload-requesting-task put itself on the task queue and it still needs
    // to be executed before the rest of the tasks.
    if (!IsMapLoaded()) {
      on_load_complete_tasks_.reserve(on_load_complete_tasks_.size() +
                                      (tasks.end() - it));
      std::move(it, tasks.end(), std::back_inserter(on_load_complete_tasks_));
      return;
    }
    std::move(*it).Run();
  }

  // Call before |OnNoBindings| as delegate can destroy this object.
  UnloadMapIfPossible();

  // We might need to call the no_bindings_callback_ here if bindings became
  // empty while waiting for load to complete.
  if (receivers_.empty())
    delegate_->OnNoBindings();
}

void StorageAreaImpl::CreateCommitBatchIfNeeded() {
  if (commit_batch_)
    return;
  DCHECK(database_);

  commit_batch_ = std::make_unique<CommitBatch>();
  StartCommitTimer();
}

void StorageAreaImpl::StartCommitTimer() {
  if (!commit_batch_)
    return;

  // Start a timer to commit any changes that accrue in the batch, but only if
  // no commits are currently in flight. In that case the timer will be
  // started after the commits have happened.
  if (commit_batches_in_flight_)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StorageAreaImpl::CommitChanges,
                     weak_ptr_factory_.GetWeakPtr()),
      ComputeCommitDelay());
}

base::TimeDelta StorageAreaImpl::ComputeCommitDelay() const {
  if (s_aggressive_flushing_enabled_)
    return base::Seconds(1);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  base::TimeDelta delay =
      std::max(default_commit_delay_,
               std::max(commit_rate_limiter_.ComputeDelayNeeded(elapsed_time),
                        data_rate_limiter_.ComputeDelayNeeded(elapsed_time)));
  // TODO(mek): Rename histogram to match class name, or eliminate histogram
  // entirely.
  UMA_HISTOGRAM_LONG_TIMES("LevelDBWrapper.CommitDelay", delay);
  return delay;
}

void StorageAreaImpl::CommitChanges() {
  // Note: commit_batch_ may be null if ScheduleImmediateCommit was called
  // after a delayed commit task was scheduled.
  if (!commit_batch_) {
    return;
  }

  database_->InitiateCommit(this);
}

std::optional<AsyncDomStorageDatabase::Commit>
StorageAreaImpl::CollectCommit() {
  if (!commit_batch_) {
    return std::nullopt;
  }

  DCHECK(database_);
  DCHECK(IsMapLoaded()) << static_cast<int>(map_state_);

  commit_rate_limiter_.add_samples(1);

  // Commit all our changes in a single batch.
  AsyncDomStorageDatabase::Commit commit;
  commit.timestamps = std::move(commit_batch_->put_timestamps);
  commit.prefix = prefix_;
  commit.clear_all_first = commit_batch_->clear_all_first;
  delegate_->PrepareToCommit(&commit.entries_to_add, &commit.keys_to_delete);

  const bool has_changes = !commit.entries_to_add.empty() ||
                           !commit.keys_to_delete.empty() ||
                           !commit_batch_->changed_values.empty() ||
                           !commit_batch_->changed_keys.empty();
  size_t data_size = 0;
  if (map_state_ == MapState::LOADED_KEYS_AND_VALUES) {
    DCHECK(commit_batch_->changed_values.empty())
        << "Map state and commit state out of sync.";
    for (const auto& key : commit_batch_->changed_keys) {
      data_size += key.size();
      DomStorageDatabase::Key prefixed_key;
      prefixed_key.reserve(prefix_.size() + key.size());
      prefixed_key.insert(prefixed_key.end(), prefix_.begin(), prefix_.end());
      prefixed_key.insert(prefixed_key.end(), key.begin(), key.end());
      auto it = keys_values_map_.find(key);
      if (it != keys_values_map_.end()) {
        data_size += it->second.size();
        commit.entries_to_add.emplace_back(std::move(prefixed_key), it->second);
      } else {
        commit.keys_to_delete.push_back(std::move(prefixed_key));
      }
    }
  } else {
    DCHECK(commit_batch_->changed_keys.empty())
        << "Map state and commit state out of sync.";
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_ONLY);
    for (auto& entry : commit_batch_->changed_values) {
      const auto& key = entry.first;
      data_size += key.size();
      DomStorageDatabase::Key prefixed_key;
      prefixed_key.reserve(prefix_.size() + key.size());
      prefixed_key.insert(prefixed_key.end(), prefix_.begin(), prefix_.end());
      prefixed_key.insert(prefixed_key.end(), key.begin(), key.end());
      auto it = keys_only_map_.find(key);
      if (it != keys_only_map_.end()) {
        data_size += entry.second.size();
        commit.entries_to_add.emplace_back(std::move(prefixed_key),
                                           std::move(entry.second));
      } else {
        commit.keys_to_delete.push_back(std::move(prefixed_key));
      }
    }
  }

  // Schedule the copy, and ignore if |clear_all_first| is specified and there
  // are no changing keys.
  if (commit_batch_->copy_to_prefix) {
    DCHECK(!has_changes);
    DCHECK(!commit_batch_->clear_all_first);
    commit.copy_to_prefix = std::move(commit_batch_->copy_to_prefix);
  }

  base::UmaHistogramCustomCounts("DOMStorage.CommitSizeBytes", data_size,
                                 /*min=*/100,
                                 /*exclusive_max=*/12 * 1024 * 1024,
                                 /*buckets=*/100);

  data_rate_limiter_.add_samples(data_size);
  commit.data_size = data_size;

  ++commit_batches_in_flight_;
  commit_batch_.reset();
  return commit;
}

void StorageAreaImpl::OnCommitComplete(leveldb::Status status) {
  has_committed_data_ = true;
  --commit_batches_in_flight_;
  StartCommitTimer();

  if (!status.ok())
    SetCacheMode(CacheMode::KEYS_AND_VALUES);

  // Call before |DidCommit| as delegate can destroy this object.
  UnloadMapIfPossible();

  delegate_->DidCommit(status);
}

void StorageAreaImpl::UnloadMapIfPossible() {
  // Do not unload the map if:
  // * The desired cache mode isn't key-only,
  // * The map isn't a loaded key-value map,
  // * There are pending tasks waiting on the key-value map being loaded, or
  // * There is no database connection.
  // * We have commit batches in-flight.
  // * We haven't committed data yet.
  if (cache_mode_ != CacheMode::KEYS_ONLY_WHEN_POSSIBLE ||
      map_state_ != MapState::LOADED_KEYS_AND_VALUES ||
      has_pending_load_tasks() || !database_ || commit_batches_in_flight_ > 0 ||
      !has_committed_data_) {
    return;
  }

  keys_only_map_.clear();
  memory_used_ = 0;
  for (auto& it : keys_values_map_) {
    keys_only_map_.insert(std::make_pair(it.first, it.second.size()));
  }
  if (commit_batch_) {
    for (const auto& key : commit_batch_->changed_keys) {
      auto value_it = keys_values_map_.find(key);
      commit_batch_->changed_values[key] = value_it == keys_values_map_.end()
                                               ? std::vector<uint8_t>()
                                               : std::move(value_it->second);
    }
    commit_batch_->changed_keys.clear();
  }

  keys_values_map_.clear();
  map_state_ = MapState::LOADED_KEYS_ONLY;

  CalculateStorageAndMemoryUsed();
}

void StorageAreaImpl::DoForkOperation(
    const base::WeakPtr<StorageAreaImpl>& forked_area) {
  if (!forked_area)
    return;

  DCHECK(IsMapLoaded());
  // TODO(dmurph): If these commits fails, then the disk could be in an
  // inconsistant state. Ideally all further operations will fail and the code
  // will correctly delete the database?
  if (database_) {
    // All changes must be stored in the database before the copy operation.
    if (has_changes_to_commit())
      CommitChanges();
    CreateCommitBatchIfNeeded();
    commit_batch_->copy_to_prefix = forked_area->prefix_;
    CommitChanges();
  }

  forked_area->OnForkStateLoaded(database_ != nullptr, keys_values_map_,
                                 keys_only_map_);
}

void StorageAreaImpl::OnForkStateLoaded(bool database_enabled,
                                        const ValueMap& value_map,
                                        const KeysOnlyMap& keys_only_map) {
  // This callback can get either the value map or the key only map depending
  // on parent operations and other things. So handle both.
  if (!value_map.empty() || keys_only_map.empty()) {
    keys_values_map_ = value_map;
    map_state_ = MapState::LOADED_KEYS_AND_VALUES;
  } else {
    keys_only_map_ = keys_only_map;
    map_state_ = MapState::LOADED_KEYS_ONLY;
  }

  if (!database_enabled) {
    database_ = nullptr;
    cache_mode_ = CacheMode::KEYS_AND_VALUES;
  }

  CalculateStorageAndMemoryUsed();
  OnLoadComplete();
}

}  // namespace storage
