// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LOCAL_STORAGE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LOCAL_STORAGE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/common/database/db_status.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class StorageServiceImpl;
// The Local Storage implementation. An instance of this class exists for each
// profile directory (within the user data directory) that is using Local
// Storage. It manages storage for all StorageKeys and namespaces within that
// partition.
class LocalStorageImpl : public base::trace_event::MemoryDumpProvider,
                         public mojom::LocalStorageControl {
 public:
  using DestructLocalStorageCallback =
      base::OnceCallback<void(LocalStorageImpl*)>;
  // Constructs a Local Storage implementation which will create its root
  // "Local Storage" directory in |storage_root| if non-empty. |task_runner|
  // run tasks on the same sequence as the one which constructs this object.
  // |legacy_task_runner| must support blocking operations and its tasks must
  // be able to block shutdown. If valid, |receiver| will be bound to this
  // object to allow for remote control via the LocalStorageControl interface.
  LocalStorageImpl(const base::FilePath& storage_root,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   DestructLocalStorageCallback destruct_callback,
                   mojo::PendingReceiver<mojom::LocalStorageControl> receiver);
  ~LocalStorageImpl() override;

  void FlushStorageKeyForTesting(const blink::StorageKey& storage_key);

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState() { force_keep_session_state_ = true; }

  // Called when the owning BrowserContext is ending.
  // Schedules the commit of any unsaved changes and will delete or keep data on
  // disk per the content settings and special storage policies.  `callback` is
  // invoked when shutdown is complete, which may happen even before ShutDown
  // returns.
  void ShutDown(base::OnceClosure callback);

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  // mojom::LocalStorageControl implementation:
  void BindStorageArea(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;
  void GetUsage(GetUsageCallback callback) override;
  void DeleteStorage(const blink::StorageKey& storage_key,
                     DeleteStorageCallback callback) override;
  void CleanUpStorage(CleanUpStorageCallback callback) override;
  void Flush() override;
  void PurgeMemory() override;
  void ApplyPolicyUpdates(
      std::vector<mojom::StoragePolicyUpdatePtr> policy_updates) override;
  void ForceKeepSessionState() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  base::FilePath GetStoragePath() const;

  // Access the underlying DomStorageDatabase. May be null if the database is
  // not yet open.
  const base::SequenceBound<DomStorageDatabase>& GetDatabaseForTesting() const {
    return database_->database();
  }

  // Wait for the database to be opened, or for opening to fail. If the database
  // is already opened, |callback| is invoked immediately.
  void SetDatabaseOpenCallbackForTesting(base::OnceClosure callback);

  void OverrideDeleteStaleStorageAreasDelayForTesting(
      const base::TimeDelta& delay);

  void ForceFakeOpenStorageAreaForTesting(const blink::StorageKey& storage_key);

 private:
  friend class DOMStorageBrowserTest;

  class StorageAreaHolder;

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progres yet.
  void RunWhenConnected(base::OnceClosure callback);

  // StorageAreas held by this LocalStorageImpl retain an unmanaged reference to
  // `database_`. This deletes them and is used any time `database_` is reset.
  void PurgeAllStorageAreas();

  // Part of our asynchronous directory opening called from RunWhenConnected().
  void InitiateConnection(bool in_memory_only = false);
  void OnDatabaseOpened(DbStatus status);
  void OnConnectionFinished();
  void DeleteAndRecreateDatabase();
  void OnDBDestroyed(bool recreate_in_memory, DbStatus status);

  StorageAreaHolder* GetOrCreateStorageArea(
      const blink::StorageKey& storage_key);

  // The (possibly delayed) implementation of GetUsage(). Can be called directly
  // from that function, or through |on_database_open_callbacks_|.
  void RetrieveStorageUsage(GetUsageCallback callback);
  void OnGotWriteMetaData(GetUsageCallback callback,
                          StatusOr<DomStorageDatabase::Metadata> all_metadata);

  void OnGotStorageUsageForShutdown(
      std::vector<mojom::StorageUsageInfoPtr> usage);
  void OnStorageKeysDeleted(DbStatus status);
  void OnShutdownComplete();

  void GetStatistics(size_t* total_cache_size, size_t* unused_area_count);
  void OnCommitResult(DbStatus status);

  // These clear stale storage areas (not read/written to within 400 days) from
  // the database. See crbug.com/40281870 for more info.
  void DeleteStaleStorageAreas();
  void OnGotMetaDataToDeleteStaleStorageAreas(
      StatusOr<DomStorageDatabase::Metadata> all_metadata);
  void OnReceiverDisconnected();

  // Passed in by the StorageServiceImpl that owns this object. Used to signal
  // that this LocalStorageImpl can be destructed when the Receiver is
  // disconnected.
  DestructLocalStorageCallback destruct_callback_;
  const base::FilePath directory_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
    CONNECTION_SHUTDOWN
  } connection_state_ = NO_CONNECTION;

  bool force_keep_session_state_ = false;

  const scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  std::unique_ptr<AsyncDomStorageDatabase> database_;
  bool tried_to_recreate_during_open_ = false;
  bool in_memory_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // Maps between a StorageKey and its prefixed LevelDB view.
  std::map<blink::StorageKey, std::unique_ptr<StorageAreaHolder>> areas_;

  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  // The set of Origins which should be cleared on shutdown.
  // this is used by ApplyPolicyUpdates to store which origin
  // to clear based on the provided StoragePolicyUpdate.
  std::set<url::Origin> origins_to_purge_on_shutdown_;

  mojo::Receiver<mojom::LocalStorageControl> control_receiver_{this};

  base::OnceClosure shutdown_complete_callback_;

  // We need to delay deleting stale storage areas until after any session
  // restore has taken place, otherwise we might fail to record current usage.
  // See crbug.com/40281870 for more info.
  base::TimeDelta delete_stale_storage_areas_delay_{base::Minutes(1)};

  base::WeakPtrFactory<LocalStorageImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LOCAL_STORAGE_IMPL_H_
