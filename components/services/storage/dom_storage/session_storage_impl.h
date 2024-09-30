// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/session_storage_namespace_impl.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

// The Session Storage implementation. An instance of this class exists for each
// storage partition using Session Storage, managing storage for all StorageKeys
// and namespaces within the partition.
class SessionStorageImpl : public base::trace_event::MemoryDumpProvider,
                           public mojom::SessionStorageControl,
                           public SessionStorageDataMap::Listener,
                           public SessionStorageNamespaceImpl::Delegate {
 public:
  enum class BackingMode {
    // Use an in-memory leveldb database to store our state.
    kNoDisk,
    // Use disk for the leveldb database, but clear its contents before we open
    // it. This is used for platforms like Android where the session restore
    // code is never used, ScavengeUnusedNamespace is never called, and old
    // session storage data will never be reused.
    kClearDiskStateOnOpen,
    // Use disk for the leveldb database, restore all saved namespaces from
    // disk. This assumes that ScavengeUnusedNamespace will eventually be called
    // to clean up unused namespaces on disk.
    kRestoreDiskState
  };

  SessionStorageImpl(
      const base::FilePath& partition_directory,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<base::SequencedTaskRunner> memory_dump_task_runner,
      BackingMode backing_option,
      std::string database_name,
      mojo::PendingReceiver<mojom::SessionStorageControl> receiver);

  ~SessionStorageImpl() override;

  // mojom::SessionStorageControl implementation:
  void BindNamespace(
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver,
      BindNamespaceCallback callback) override;
  void BindStorageArea(
      const blink::StorageKey& storage_key,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver,
      BindStorageAreaCallback callback) override;
  void GetUsage(GetUsageCallback callback) override;
  void DeleteStorage(const blink::StorageKey& storage_key,
                     const std::string& namespace_id,
                     DeleteStorageCallback callback) override;
  void CleanUpStorage(CleanUpStorageCallback callback) override;
  void ScavengeUnusedNamespaces(
      ScavengeUnusedNamespacesCallback callback) override;
  void Flush() override;
  void PurgeMemory() override;
  void CreateNamespace(const std::string& namespace_id) override;
  void CloneNamespace(const std::string& namespace_id_to_clone,
                      const std::string& clone_namespace_id,
                      mojom::SessionStorageCloneType clone_type) override;
  void DeleteNamespace(const std::string& namespace_id,
                       bool should_persist) override;

  // Called when the client (i.e. the corresponding browser storage partition)
  // disconnects. Schedules the commit of any unsaved changes. All data on disk
  // (where there was no call to DeleteNamespace will stay on disk for later
  // restoring. `callback` is invoked when shutdown is complete, which may
  // happen even before ShutDown returns.
  void ShutDown(base::OnceClosure callback);

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void PretendToConnectForTesting();

  AsyncDomStorageDatabase* DatabaseForTesting() { return database_.get(); }

  void FlushAreaForTesting(const std::string& namespace_id,
                           const blink::StorageKey& storage_key);

  // Access the underlying DomStorageDatabase. May be null if the database is
  // not yet open.
  base::SequenceBound<DomStorageDatabase>& GetDatabaseForTesting() {
    return database_->database();
  }

  const SessionStorageMetadata& GetMetadataForTesting() const {
    return metadata_;
  }

  SessionStorageNamespaceImpl* GetNamespaceForTesting(const std::string& id) {
    auto it = namespaces_.find(id);
    if (it == namespaces_.end())
      return nullptr;
    return it->second.get();
  }

  // Wait for the database to be opened, or for opening to fail. If the database
  // is already opened, |callback| is invoked immediately.
  void SetDatabaseOpenCallbackForTesting(base::OnceClosure callback);

 private:
  friend class DOMStorageBrowserTest;

  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class OpenResult {
    kDirectoryOpenFailed = 0,
    kDatabaseOpenFailed = 1,
    kInvalidVersion = 2,
    kVersionReadError = 3,
    kNamespacesReadError = 4,
    kSuccess = 6,
    kMaxValue = kSuccess
  };

  scoped_refptr<SessionStorageMetadata::MapData> RegisterNewAreaMap(
      SessionStorageMetadata::NamespaceEntry namespace_entry,
      const blink::StorageKey& storage_key);

  // SessionStorageAreaImpl::Listener implementation:
  void OnDataMapCreation(const std::vector<uint8_t>& map_prefix,
                         SessionStorageDataMap* map) override;
  void OnDataMapDestruction(const std::vector<uint8_t>& map_prefix) override;
  void OnCommitResult(leveldb::Status status) override;
  void OnCommitResultWithCallback(base::OnceClosure callback,
                                  leveldb::Status status);

  // SessionStorageNamespaceImpl::Delegate implementation:
  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      const std::vector<uint8_t>& map_number_as_bytes) override;
  void RegisterShallowClonedNamespace(
      SessionStorageMetadata::NamespaceEntry source_namespace_entry,
      const std::string& new_namespace_id,
      const SessionStorageNamespaceImpl::StorageKeyAreas&
          clone_from_storage_keys) override;

  std::unique_ptr<SessionStorageNamespaceImpl>
  CreateSessionStorageNamespaceImpl(std::string namespace_id);

  void DoDatabaseDelete(const std::string& namespace_id);

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progress yet.
  void RunWhenConnected(base::OnceClosure callback);

  // Part of our asynchronous directory opening called from RunWhenConnected().
  void InitiateConnection(bool in_memory_only = false);
  void OnDatabaseOpened(leveldb::Status status);

  struct ValueAndStatus {
    ValueAndStatus();
    ValueAndStatus(ValueAndStatus&&);
    ~ValueAndStatus();
    leveldb::Status status;
    DomStorageDatabase::Value value;
  };

  struct KeyValuePairsAndStatus {
    KeyValuePairsAndStatus();
    KeyValuePairsAndStatus(KeyValuePairsAndStatus&&);
    ~KeyValuePairsAndStatus();
    leveldb::Status status;
    std::vector<DomStorageDatabase::KeyValuePair> key_value_pairs;
  };

  void OnGotDatabaseMetadata(ValueAndStatus version,
                             KeyValuePairsAndStatus namespaces,
                             ValueAndStatus next_map_id);

  struct MetadataParseResult {
    OpenResult open_result;
    const char* histogram_name;
  };
  MetadataParseResult ParseDatabaseVersion(
      ValueAndStatus version,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask>* migration_tasks);
  MetadataParseResult ParseNamespaces(
      KeyValuePairsAndStatus namespaces,
      std::vector<AsyncDomStorageDatabase::BatchDatabaseTask> migration_tasks);
  MetadataParseResult ParseNextMapId(ValueAndStatus next_map_id);

  void OnConnectionFinished();
  void PurgeAllNamespaces();
  void DeleteAndRecreateDatabase(const char* histogram_name);
  void OnDBDestroyed(bool recreate_in_memory, leveldb::Status status);

  void OnShutdownComplete();

  void GetStatistics(size_t* total_cache_size, size_t* unused_areas_count);

  void LogDatabaseOpenResult(OpenResult result);

  // Since the session storage object hierarchy references iterators owned by
  // the metadata, make sure it is destroyed last on destruction.
  SessionStorageMetadata metadata_;

  BackingMode backing_mode_;
  std::string database_name_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
    CONNECTION_SHUTDOWN
  } connection_state_ = NO_CONNECTION;
  bool database_initialized_ = false;

  const base::FilePath partition_directory_;
  const scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  mojo::Receiver<mojom::SessionStorageControl> receiver_;

  std::unique_ptr<AsyncDomStorageDatabase> database_;
  bool in_memory_ = false;
  bool tried_to_recreate_during_open_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // The removal of items from this map is managed by the refcounting in
  // SessionStorageDataMap.
  // Populated after the database is connected.
  std::map<std::vector<uint8_t>,
           raw_ptr<SessionStorageDataMap, CtnExperimental>>
      data_maps_;
  // Populated in CreateNamespace, CloneNamespace, and sometimes
  // RegisterShallowClonedNamespace. Items are removed in
  // DeleteNamespace.
  std::map<std::string, std::unique_ptr<SessionStorageNamespaceImpl>>
      namespaces_;

  // Scavenging only happens once.
  bool has_scavenged_ = false;
  // When namespaces are destroyed but marked as persistent, a scavenge should
  // not delete them. Cleared after ScavengeUnusedNamespaces is called.
  std::set<std::string> protected_namespaces_from_scavenge_;

  bool is_low_end_mode_;
  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  // Name of an extra histogram to log open results to, if not null.
  const char* open_result_histogram_ = nullptr;

  base::OnceClosure shutdown_complete_callback_;

  base::WeakPtrFactory<SessionStorageImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_IMPL_H_
