// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace storage {
class AsyncDomStorageDatabase;

BASE_DECLARE_FEATURE(kDomStorageSmartFlushing);

// This is a wrapper around a AsyncDomStorageDatabase. Multiple interface
// endpoints can be bound to the same object. The wrapper adds a couple of
// features not found directly in leveldb:
//
// 1) Adds the given prefix, if any, to all keys. This allows the sharing of one
//    database across many, possibly untrusted, consumers and ensuring that they
//    can't access each other's values.
// 2) Enforces a max_size constraint.
// 3) Informs observers when values scoped by prefix are modified.
// 4) Throttles requests to avoid overwhelming the disk.
//
// The wrapper supports two different caching modes.
class StorageAreaImpl : public blink::mojom::StorageArea,
                        public AsyncDomStorageDatabase::Committer {
 public:
  using ValueMap = std::map<std::vector<uint8_t>, std::vector<uint8_t>>;
  using ValueMapCallback = base::OnceCallback<void(std::unique_ptr<ValueMap>)>;
  using Change =
      std::pair<std::vector<uint8_t>, std::optional<std::vector<uint8_t>>>;
  using KeysOnlyMap = std::map<std::vector<uint8_t>, size_t>;

  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnNoBindings() = 0;
    virtual void PrepareToCommit(
        std::vector<DomStorageDatabase::KeyValuePair>* extra_entries_to_add,
        std::vector<DomStorageDatabase::Key>* extra_keys_to_delete);
    virtual void DidCommit(leveldb::Status error) = 0;
    virtual void OnMapLoaded(leveldb::Status status);
  };

  enum class CacheMode {
    // The cache stores only keys (required to maintain max size constraints)
    // when there is only one client binding to save memory. The client is
    // asked to send old values on mutations for sending notifications to
    // observers.
    KEYS_ONLY_WHEN_POSSIBLE,
    // The cache always stores keys and values.
    KEYS_AND_VALUES
  };

  // Options provided to constructor.
  struct Options {
    CacheMode cache_mode = CacheMode::KEYS_AND_VALUES;

    // Max bytes of storage that can be used by key value pairs.
    size_t max_size = 0;
    // Minimum time between 2 commits to disk.
    base::TimeDelta default_commit_delay;
    // Maximum number of bytes written to disk in one hour.
    int max_bytes_per_hour = 0;
    // Maximum number of disk write batches in one hour.
    int max_commits_per_hour = 0;
  };

  // |Delegate::OnNoBindings| will be called when this object has no more
  // bindings and all pending modifications have been processed.
  StorageAreaImpl(AsyncDomStorageDatabase* database,
                  const std::string& prefix,
                  Delegate* delegate,
                  const Options& options);
  StorageAreaImpl(AsyncDomStorageDatabase* database,
                  std::vector<uint8_t> prefix,
                  Delegate* delegate,
                  const Options& options);

  StorageAreaImpl(const StorageAreaImpl&) = delete;
  StorageAreaImpl& operator=(const StorageAreaImpl&) = delete;

  ~StorageAreaImpl() override;

  // Initializes the storage area as loaded & empty. This can only be called
  // immediately after construction, and before any other methods are called
  // that would load data from the database.
  // This avoids hitting disk to load a map that the implementer already knows
  // must be empty. Do not use this option unless you are absolutely certain
  // that there must be no data for the |prefix|, as the data will not be loaded
  // to check.
  void InitializeAsEmpty();

  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  // Forks, or copies, all data in this prefix to another prefix.
  // Note: this object (the parent) must stay alive until the forked area
  // has been loaded (see initialized()).
  std::unique_ptr<StorageAreaImpl> ForkToNewPrefix(
      const std::string& new_prefix,
      Delegate* delegate,
      const Options& options);
  std::unique_ptr<StorageAreaImpl> ForkToNewPrefix(
      std::vector<uint8_t> new_prefix,
      Delegate* delegate,
      const Options& options);

  // Cancels all pending load tasks. Useful for emergency destructions. If the
  // area is unloaded (initialized() returns false), this will DROP all
  // pending changes to the database, and any uninitialized areas created
  // through |ForkToNewPrefix| will stay BROKEN and unresponsive.
  void CancelAllPendingRequests();

  // The total bytes used by items which counts towards the quota.
  size_t storage_used() const { return storage_used_; }
  // The physical memory used by the cache.
  size_t memory_used() const { return memory_used_; }

  bool empty() const { return storage_used_ == 0; }

  // If this ares is loaded and sending changes to the database.
  bool initialized() const { return IsMapLoaded(); }

  CacheMode cache_mode() const { return cache_mode_; }

  // Tasks that are waiting for the map to be loaded.
  bool has_pending_load_tasks() const {
    return !on_load_complete_tasks_.empty();
  }

  bool has_changes_to_commit() const { return commit_batch_.get(); }

  const std::vector<uint8_t>& prefix() { return prefix_; }

  AsyncDomStorageDatabase* database() { return database_; }

  // Commence aggressive flushing. This should be called early during startup,
  // before any localStorage writing. Currently scheduled writes will not be
  // rescheduled and will be flushed at the scheduled time after which
  // aggressive flushing will commence.
  static void EnableAggressiveCommitDelay();

  // Commits any uncommitted data to the database as soon as possible. This
  // usually means data will be committed immediately, but if we're currently
  // waiting on the result of initializing our map the commit won't happen
  // until the load has finished. If provided, |callback| is run only once the
  // commit is fully completed.
  void ScheduleImmediateCommit();

  // Clears the in-memory cache if currently no changes are pending. If there
  // are uncommitted changes this method does nothing.
  void PurgeMemory();

  // Adds memory statistics to |pmd| for memory infra.
  void OnMemoryDump(const std::string& name,
                    base::trace_event::ProcessMemoryDump* pmd);

  // Sets cache mode to either store only keys or keys and values. See
  // SetCacheMode().
  void SetCacheModeForTesting(CacheMode cache_mode);

  // blink::mojom::StorageArea:
  void AddObserver(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> observer) override;
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const std::optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              const std::optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override;
  void DeleteAll(
      const std::string& source,
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      DeleteAllCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetAll(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      GetAllCallback callback) override;
  void Checkpoint() override;

  // Committer:
  std::optional<AsyncDomStorageDatabase::Commit> CollectCommit() override;
  base::OnceCallback<void(leveldb::Status)> GetCommitCompleteCallback()
      override;

  void OnCommitComplete(leveldb::Status status);

  void SetOnLoadCallbackForTesting(base::OnceClosure callback) {
    on_load_callback_for_testing_ = std::move(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(StorageAreaImplTest, GetAllAfterSetCacheMode);
  FRIEND_TEST_ALL_PREFIXES(StorageAreaImplTest,
                           PutLoadsValuesAfterCacheModeUpgrade);
  FRIEND_TEST_ALL_PREFIXES(StorageAreaImplTest, SetCacheModeConsistent);
  FRIEND_TEST_ALL_PREFIXES(StorageAreaImplCacheModeTest,
                           CommitOnDifferentCacheModes);

  // Used to rate limit commits.
  class RateLimiter {
   public:
    RateLimiter(size_t desired_rate, base::TimeDelta time_quantum);

    void add_samples(size_t samples) { samples_ += samples; }

    // Computes the total time needed to process the total samples seen
    // at the desired rate.
    base::TimeDelta ComputeTimeNeeded() const;

    // Given the elapsed time since the start of the rate limiting session,
    // computes the delay needed to mimic having processed the total samples
    // seen at the desired rate.
    base::TimeDelta ComputeDelayNeeded(
        const base::TimeDelta elapsed_time) const;

    float rate() const { return rate_; }

   private:
    float rate_;
    float samples_;
    base::TimeDelta time_quantum_;
  };

  // There can be only one fork operation per commit batch.
  struct CommitBatch {
    CommitBatch();
    ~CommitBatch();

    bool clear_all_first = false;
    // Prefix copying is performed before applying changes.
    std::optional<std::vector<uint8_t>> copy_to_prefix;
    // Used if the map_type_ is LOADED_KEYS_ONLY.
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> changed_values;
    // Used if the map_type_ is LOADED_KEYS_AND_VALUES.
    std::set<std::vector<uint8_t>> changed_keys;
    // Timestamp of each discrete `Put()` call that was coalesced into this
    // batch.
    std::vector<base::TimeTicks> put_timestamps;
  };

  enum class MapState {
    UNLOADED,
    // Loading from the database connection.
    LOADING_FROM_DATABASE,
    // Loading from another StorageAreaImpl that we have forked from.
    LOADING_FROM_FORK,
    LOADED_KEYS_ONLY,
    LOADED_KEYS_AND_VALUES
  };

  using LoadStateForForkCallback = base::OnceCallback<
      void(bool database_enabled, const ValueMap&, const KeysOnlyMap&)>;
  using ForkSourceEarlyDeathCallback =
      base::OnceCallback<void(std::vector<uint8_t> source_prefix)>;

  // Changes the cache mode of the area. If applicable, this will change the
  // internal storage type after the next commit. The keys-only mode can only
  // be set only when there is one client binding. It automatically changes to
  // keys-and-values mode when more than one binding exists.
  // Notifications to observers when an item is mutated depends on the
  // |client_old_value| when in keys-only mode. Using GetAll during
  // keys-only mode will cause extra disk access.
  void SetCacheMode(CacheMode cache_mode);

  void OnConnectionError();

  // Always loads the |keys_values_map_|, sets the |map_state_| to
  // LOADED_KEYS_AND_VALUES, and calls through all the completion callbacks.
  //
  // Then if the |cache_mode_| is keys-only, it unloads the map to the
  // |keys_only_map_| and sets the |map_state_| to LOADED_KEYS_ONLY
  void LoadMap(base::OnceClosure completion_callback);
  void OnMapLoaded(leveldb::Status status,
                   std::vector<DomStorageDatabase::KeyValuePair> data);
  void CalculateStorageAndMemoryUsed();
  void OnLoadComplete();

  void CreateCommitBatchIfNeeded();
  void StartCommitTimer();
  base::TimeDelta ComputeCommitDelay() const;

  void CommitChanges();

  void UnloadMapIfPossible();

  bool IsMapUpgradeNeeded() const {
    return map_state_ == MapState::LOADED_KEYS_ONLY &&
           cache_mode_ == CacheMode::KEYS_AND_VALUES;
  }

  bool IsMapLoaded() const {
    return map_state_ == MapState::LOADED_KEYS_ONLY ||
           map_state_ == MapState::LOADED_KEYS_AND_VALUES;
  }

  bool IsMapLoadedAndEmpty() const {
    return (map_state_ == MapState::LOADED_KEYS_ONLY &&
            keys_only_map_.empty()) ||
           (map_state_ == MapState::LOADED_KEYS_AND_VALUES &&
            keys_values_map_.empty());
  }

  void DoForkOperation(const base::WeakPtr<StorageAreaImpl>& forked_area);
  void OnForkStateLoaded(bool database_enabled,
                         const ValueMap& map,
                         const KeysOnlyMap& key_only_map);

  std::vector<uint8_t> prefix_;
  mojo::ReceiverSet<blink::mojom::StorageArea> receivers_;
  mojo::RemoteSet<blink::mojom::StorageAreaObserver> observers_;
  raw_ptr<Delegate, DanglingUntriaged> delegate_;
  raw_ptr<AsyncDomStorageDatabase> database_;

  // For commits to work correctly the map loaded state (keys vs keys & values)
  // must stay consistent for a given commit batch.
  MapState map_state_ = MapState::UNLOADED;
  CacheMode cache_mode_;
  ValueMap keys_values_map_;
  KeysOnlyMap keys_only_map_;
  // These are always consumed & cleared when the map is loaded.
  std::vector<base::OnceClosure> on_load_complete_tasks_;

  size_t storage_used_;
  size_t max_size_;
  size_t memory_used_;
  base::TimeTicks start_time_;
  base::TimeDelta default_commit_delay_;
  RateLimiter data_rate_limiter_;
  RateLimiter commit_rate_limiter_;
  int commit_batches_in_flight_ = 0;
  bool has_committed_data_ = false;
  std::unique_ptr<CommitBatch> commit_batch_;

  base::OnceClosure on_load_callback_for_testing_;

  base::WeakPtrFactory<StorageAreaImpl> weak_ptr_factory_{this};

  static bool s_aggressive_flushing_enabled_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_STORAGE_AREA_IMPL_H_
