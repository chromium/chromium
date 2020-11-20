// Copyright 2016 The Chromium Authors. All rights reserved.
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

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace storage {

// The Local Storage implementation. An instance of this class exists for each
// storage partition using Local Storage, managing storage for all origins
// within the partition.
class LocalStorageImpl : public base::trace_event::MemoryDumpProvider,
                         public mojom::LocalStorageControl {
 public:
  static base::FilePath LegacyDatabaseFileNameFromOrigin(
      const url::Origin& origin);
  static url::Origin OriginFromLegacyDatabaseFileName(
      const base::FilePath& file_name);

  // Constructs a Local Storage implementation which will create its root
  // "Local Storage" directory in |storage_root| if non-empty. |task_runner|
  // run tasks on the same sequence as the one which constructs this object.
  // |legacy_task_runner| must support blocking operations and its tasks must
  // be able to block shutdown. If valid, |receiver| will be bound to this
  // object to allow for remote control via the LocalStorageControl interface.
  LocalStorageImpl(const base::FilePath& storage_root,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   scoped_refptr<base::SequencedTaskRunner> legacy_task_runner,
                   mojo::PendingReceiver<mojom::LocalStorageControl> receiver);

  void FlushOriginForTesting(const url::Origin& origin);

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState() { force_keep_session_state_ = true; }

  // Called when the owning BrowserContext is ending.
  // Schedules the commit of any unsaved changes and will delete
  // and keep data on disk per the content settings and special storage
  // policies.
  void ShutdownAndDelete();

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  // mojom::LocalStorageControl implementation:
  void BindStorageArea(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;
  void GetUsage(GetUsageCallback callback) override;
  void DeleteStorage(const url::Origin& origin,
                     DeleteStorageCallback callback) override;
  void CleanUpStorage(CleanUpStorageCallback callback) override;
  void Flush(FlushCallback callback) override;
  void PurgeMemory() override;
  void ApplyPolicyUpdates(
      std::vector<mojom::LocalStoragePolicyUpdatePtr> policy_updates) override;
  void ForceKeepSessionState() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Converts a string from the old storage format to the new storage format.
  static std::vector<uint8_t> MigrateString(const base::string16& input);

  // Access the underlying DomStorageDatabase. May be null if the database is
  // not yet open.
  const base::SequenceBound<DomStorageDatabase>& GetDatabaseForTesting() const {
    return database_->database();
  }

  // Wait for the database to be opened, or for opening to fail. If the database
  // is already opened, |callback| is invoked immediately.
  void SetDatabaseOpenCallbackForTesting(base::OnceClosure callback);

 private:
  friend class DOMStorageBrowserTest;

  class StorageAreaHolder;

  ~LocalStorageImpl() override;

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progres yet.
  void RunWhenConnected(base::OnceClosure callback);

  // Part of our asynchronous directory opening called from RunWhenConnected().
  void InitiateConnection(bool in_memory_only = false);
  void OnDatabaseOpened(leveldb::Status status);
  void OnGotDatabaseVersion(leveldb::Status status,
                            const std::vector<uint8_t>& value);
  void OnConnectionFinished();
  void DeleteAndRecreateDatabase(const char* histogram_name);
  void OnDBDestroyed(bool recreate_in_memory, leveldb::Status status);

  StorageAreaHolder* GetOrCreateStorageArea(const url::Origin& origin);

  // The (possibly delayed) implementation of GetUsage(). Can be called directly
  // from that function, or through |on_database_open_callbacks_|.
  void RetrieveStorageUsage(GetUsageCallback callback);
  void OnGotMetaData(GetUsageCallback callback,
                     std::vector<DomStorageDatabase::KeyValuePair> data);

  void OnGotStorageUsageForShutdown(
      std::vector<mojom::LocalStorageUsageInfoPtr> usage);
  void OnShutdownComplete(leveldb::Status status);

  void GetStatistics(size_t* total_cache_size, size_t* unused_area_count);
  void OnCommitResult(leveldb::Status status);

  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class OpenResult {
    DIRECTORY_OPEN_FAILED = 0,
    DATABASE_OPEN_FAILED = 1,
    INVALID_VERSION = 2,
    VERSION_READ_ERROR = 3,
    SUCCESS = 4,
    MAX
  };

  void LogDatabaseOpenResult(OpenResult result);

  const base::FilePath directory_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
    CONNECTION_SHUTDOWN
  } connection_state_ = NO_CONNECTION;
  bool database_initialized_ = false;

  bool force_keep_session_state_ = false;

  const scoped_refptr<base::SequencedTaskRunner> leveldb_task_runner_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  std::unique_ptr<AsyncDomStorageDatabase> database_;
  bool tried_to_recreate_during_open_ = false;
  bool in_memory_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // Maps between an origin and its prefixed LevelDB view.
  std::map<url::Origin, std::unique_ptr<StorageAreaHolder>> areas_;

  // Used to access old data for migration.
  scoped_refptr<base::SequencedTaskRunner> legacy_task_runner_;

  bool is_low_end_device_;
  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  // The set of (origin) URLs whose storage should be cleared on shutdown.
  std::set<GURL> origins_to_purge_on_shutdown_;

  // Name of an extra histogram to log open results to, if not null.
  const char* open_result_histogram_ = nullptr;

  mojo::Receiver<mojom::LocalStorageControl> control_receiver_{this};

  base::WeakPtrFactory<LocalStorageImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LOCAL_STORAGE_IMPL_H_
