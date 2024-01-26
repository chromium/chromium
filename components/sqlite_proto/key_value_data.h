// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_PROTO_KEY_VALUE_DATA_H_
#define COMPONENTS_SQLITE_PROTO_KEY_VALUE_DATA_H_

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/table_manager.h"

namespace sqlite_proto {

namespace internal {
// FakeCompare is a dummy comparator provided so that clients using
// KeyValueData<T> objects with unbounded-size caches need not
// specify the Compare template parameter, which is used exclusively
// for pruning the cache when it would exceed its size bound.
template <typename T>
struct FakeCompare {
  bool operator()(const T& unused_lhs, const T& unused_rhs) { return true; }
};
}  // namespace internal

// The class provides a synchronous access to the data backed by
// KeyValueTable<T>. The current implementation caches all the
// data in the memory. The cache size is limited by the |max_num_entries|
// parameter, using the Compare function to decide which entries should
// be evicted. The data is written back to disk periodically and some updates
// might be lost on shutdown. To prevent this data loss, one can call
// `FlushDataToDisk()` before shutting down.
//
// NOTE: If the data store is larger than the maximum cache size, it
// will be pruned on construction to satisfy the size invariant specified
// by |max_num_entries|. If this is undesirable, set a sufficiently high
// |max_num_entries| (or pass |max_num_entries| = std::nullopt for
// unbounded size).
//
// InitializeOnDBSequence() must be called on the DB sequence of the
// TableManager. All other methods must be called on UI thread.
template <typename T, typename Compare = internal::FakeCompare<T>>
class KeyValueData {
 public:
  // Constructor. Parameters:
  // - |manager| provides an interface for scheduling database tasks for
  // execution on the database thread.
  // - |backend| provides the operations for updating and querying the
  // backing database (to be scheduled and executed using |manager|).
  // - |max_num_entries|, if given, caps the size of the in-memory cache;
  // the Compare template parameter requires a meaningful (in particular,
  // non-default) value iff max_num_entries is non-nullopt.
  // - |flush_delay| is the interval for which to gather writes and deletes
  // passing them through to the backing store; a value of zero will
  // pass writes and deletes through immediately.
  KeyValueData(scoped_refptr<TableManager> manager,
               KeyValueTable<T>* backend,
               std::optional<size_t> max_num_entries,
               base::TimeDelta flush_delay);

  KeyValueData(const KeyValueData&) = delete;
  KeyValueData& operator=(const KeyValueData&) = delete;

  // Must be called on the provided TableManager's DB sequence
  // before calling all other methods.
  void InitializeOnDBSequence();

  // Assigns data associated with the |key| to |data|. Returns true iff the
  // |key| exists, false otherwise. |data| pointer may be nullptr to get the
  // return value only.
  bool TryGetData(const std::string& key, T* data) const;

  // Returns a view of all the cached data. (The next write or delete may
  // invalidate this reference.)
  const std::map<std::string, T>& GetAllCached() { return *data_cache_; }

  // Assigns data associated with the |key| to |data|.
  void UpdateData(const std::string& key, const T& data);

  // Deletes data associated with the |keys| from the database.
  void DeleteData(const std::vector<std::string>& keys);

  // Deletes all entries from the database.
  void DeleteAllData();

  // Force cached updates to be immediately flushed to disk.
  void FlushDataToDisk();

  // As above, but runs `on_done` when all tasks have finished flushing to disk,
  // including any previously posted tasks.
  void FlushDataToDisk(base::OnceClosure on_done);

 private:
  struct EntryCompare : private Compare {
    bool operator()(const std::pair<std::string, T>& lhs,
                    const std::pair<std::string, T>& rhs) {
      return Compare::operator()(lhs.second, rhs.second);
    }
  };

  enum class DeferredOperation { kUpdate, kDelete };

  scoped_refptr<TableManager> manager_;
  base::WeakPtr<KeyValueTable<T>> backend_table_;
  std::unique_ptr<std::map<std::string, T>> data_cache_;
  std::unordered_map<std::string, DeferredOperation> deferred_updates_;
  base::RepeatingTimer flush_timer_;
  const base::TimeDelta flush_delay_;
  const std::optional<size_t> max_num_entries_;
  EntryCompare entry_compare_;

  SEQUENCE_CHECKER(sequence_checker_);
};

template <typename T, typename Compare>
KeyValueData<T, Compare>::KeyValueData(scoped_refptr<TableManager> manager,
                                       KeyValueTable<T>* backend,
                                       std::optional<size_t> max_num_entries,
                                       base::TimeDelta flush_delay)
    : manager_(manager),
      backend_table_(backend->AsWeakPtr()),
      flush_delay_(flush_delay),
      max_num_entries_(max_num_entries) {}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::InitializeOnDBSequence() {
  DCHECK(manager_->GetTaskRunner()->RunsTasksInCurrentSequence());
  auto data_map = std::make_unique<std::map<std::string, T>>();

  manager_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &KeyValueTable<T>::GetAllData, backend_table_, data_map.get()));

  // To ensure invariant that data_cache_.size() <= max_num_entries_.
  std::vector<std::string> keys_to_delete;
  while (max_num_entries_.has_value() && data_map->size() > *max_num_entries_) {
    auto entry_to_delete =
        std::min_element(data_map->begin(), data_map->end(), entry_compare_);
    keys_to_delete.emplace_back(entry_to_delete->first);
    data_map->erase(entry_to_delete);
  }
  if (!keys_to_delete.empty()) {
    manager_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&KeyValueTable<T>::DeleteData, backend_table_,
                       std::vector<std::string>(keys_to_delete)));
  }

  data_cache_ = std::move(data_map);
}

template <typename T, typename Compare>
bool KeyValueData<T, Compare>::TryGetData(const std::string& key,
                                          T* data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_cache_);
  auto it = data_cache_->find(key);
  if (it == data_cache_->end())
    return false;

  if (data)
    *data = it->second;
  return true;
}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::UpdateData(const std::string& key,
                                          const T& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_cache_);
  auto it = data_cache_->find(key);
  if (it == data_cache_->end()) {
    if (data_cache_->size() == max_num_entries_) {
      auto entry_to_delete = std::min_element(
          data_cache_->begin(), data_cache_->end(), entry_compare_);
      deferred_updates_[entry_to_delete->first] = DeferredOperation::kDelete;
      data_cache_->erase(entry_to_delete);
    }
    data_cache_->emplace(key, data);
  } else {
    it->second = data;
  }
  deferred_updates_[key] = DeferredOperation::kUpdate;

  if (flush_delay_.is_zero()) {
    // Flush immediately, only for tests.
    FlushDataToDisk();
  } else if (!flush_timer_.IsRunning()) {
    flush_timer_.Start(FROM_HERE, flush_delay_, this,
                       &KeyValueData::FlushDataToDisk);
  }
}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::DeleteData(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_cache_);
  for (const std::string& key : keys) {
    if (data_cache_->erase(key))
      deferred_updates_[key] = DeferredOperation::kDelete;
  }

  // Run all deferred updates immediately because it was requested by user.
  if (!deferred_updates_.empty())
    FlushDataToDisk();
}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::DeleteAllData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_cache_);
  data_cache_->clear();
  deferred_updates_.clear();
  // Delete all the content of the database immediately because it was requested
  // by user.
  manager_->ScheduleDBTask(
      FROM_HERE,
      base::BindOnce(&KeyValueTable<T>::DeleteAllData, backend_table_));
}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::FlushDataToDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (deferred_updates_.empty())
    return;

  std::vector<std::string> keys_to_delete;
  for (const auto& entry : deferred_updates_) {
    const std::string& key = entry.first;
    switch (entry.second) {
      case DeferredOperation::kUpdate: {
        auto it = data_cache_->find(key);
        if (it != data_cache_->end()) {
          manager_->ScheduleDBTask(
              FROM_HERE, base::BindOnce(&KeyValueTable<T>::UpdateData,
                                        backend_table_, key, it->second));
        }
        break;
      }
      case DeferredOperation::kDelete:
        keys_to_delete.push_back(key);
    }
  }

  if (!keys_to_delete.empty()) {
    manager_->ScheduleDBTask(
        FROM_HERE, base::BindOnce(&KeyValueTable<T>::DeleteData, backend_table_,
                                  keys_to_delete));
  }

  deferred_updates_.clear();
}

template <typename T, typename Compare>
void KeyValueData<T, Compare>::FlushDataToDisk(base::OnceClosure on_done) {
  FlushDataToDisk();

  // Wait for all tasks posted to the task runner before now to complete. This
  // accounts for any previously scheduled updates as well.
  manager_->ScheduleDBTaskWithReply(FROM_HERE, base::DoNothing(),
                                    std::move(on_done));
}

}  // namespace sqlite_proto

#endif  // COMPONENTS_SQLITE_PROTO_KEY_VALUE_DATA_H_
