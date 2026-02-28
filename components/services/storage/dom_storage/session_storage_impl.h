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
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/session_storage_namespace_impl.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class StorageServiceImpl;
// The Session Storage implementation. An instance of this class exists for each
// profile directory (within the user data directory) that is using Session
// Storage. It manages storage for all StorageKeys and namespaces within that
// partition.
class SessionStorageImpl : public base::trace_event::MemoryDumpProvider,
                           public mojom::SessionStorageControl,
                           public SessionStorageDataMap::Listener,
                           public SessionStorageNamespaceImpl::Delegate {
 public:
  enum class BackingMode {
    // Use an in-memory database to store our state.
    kNoDisk,
    // Use disk for the database, but clear its contents before we open
    // it. This is used for platforms like Android where the session restore
    // code is never used, ScavengeUnusedNamespace is never called, and old
    // session storage data will never be reused.
    kClearDiskStateOnOpen,
    // Use disk for the database, restore all saved namespaces from
    // disk. This assumes that ScavengeUnusedNamespace will eventually be called
    // to clean up unused namespaces on disk.
    kRestoreDiskState
  };

  using DestructSessionStorageCallback =
      base::OnceCallback<void(SessionStorageImpl*)>;
  SessionStorageImpl(
      const base::FilePath& storage_partition_directory,
      BackingMode backing_option,
      DestructSessionStorageCallback destruct_callback,
      mojo::PendingReceiver<mojom::SessionStorageControl> receiver);

  ~SessionStorageImpl() override;

  // mojom::SessionStorageControl implementation:
  void BindNamespace(
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver)
      override;
  void BindStorageArea(
      const blink::StorageKey& storage_key,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;
  void GetUsage(GetUsageCallback callback) override;
  void DeleteStorage(const blink::StorageKey& storage_key,
                     const std::string& namespace_id,
                     DeleteStorageCallback callback) override;
  void CleanUpStorage(CleanUpStorageCallback callback) override;
  void ScavengeUnusedNamespaces() override;
  void Flush() override;
  void PurgeMemory() override;
  void CreateNamespace(const std::string& namespace_id) override;
  void CloneNamespace(const std::string& namespace_id_to_clone,
                      const std::string& clone_namespace_id,
                      mojom::SessionStorageCloneType clone_type) override;
  void DeleteNamespace(const std::string& namespace_id,
                       bool should_persist) override;

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const base::FilePath& GetStoragePartitionDirectory() const;

  void FlushAreaForTesting(const std::string& namespace_id,
                           const blink::StorageKey& storage_key);

  // Access the underlying `AsyncDomStorageDatabase`. May be null if the
  // database is not yet open.
  AsyncDomStorageDatabase* GetDatabaseForTesting() { return database_.get(); }

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

  // Constructs an absolute path to the database using
  // `storage_partition_directory_`.
  base::FilePath GetDatabasePath() const;

  scoped_refptr<DomStorageDatabase::SharedMapLocator> RegisterNewAreaMap(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key);

  // SessionStorageAreaImpl::Listener implementation:
  void OnDataMapCreation(int64_t map_id, SessionStorageDataMap* map) override;
  void OnDataMapDestruction(int64_t map_id) override;
  void OnCommitResult(DbStatus status) override;

  // SessionStorageNamespaceImpl::Delegate implementation:
  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      int64_t map_id) override;
  void RegisterShallowClonedNamespace(
      const std::string& source_namespace_id,
      const std::string& new_namespace_id,
      const SessionStorageNamespaceImpl::StorageKeyAreas&
          clone_from_storage_keys) override;

  std::unique_ptr<SessionStorageNamespaceImpl>
  CreateSessionStorageNamespaceImpl(std::string namespace_id);

  // Removes the namespaces in `namespace_ids` from `metadata_` and `database_`.
  // Deletes map key/value pairs from `database_` for maps that no longer have
  // any references.
  void DeleteNamespacesFromMetadataAndDatabase(
      std::vector<std::string> namespace_ids);

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progress yet.
  void RunWhenConnected(base::OnceClosure callback);

  // Part of asynchronous database opening called from `RunWhenConnected()`. If
  // opening the database on disk fails twice, falls back to in memory. If
  // opening the database in memory fails, runs without a database.
  void InitiateConnection(bool in_memory_only = false);
  void OnDatabaseOpened(DbStatus status);
  void OnGotDatabaseMetadata(
      StatusOr<DomStorageDatabase::Metadata> all_metadata);
  void OnConnectionFinished();
  void PurgeAllNamespaces();
  void DeleteAndRecreateDatabase();
  void OnDBDestroyed(bool recreate_in_memory, DbStatus status);

  void GetStatistics(size_t* total_cache_size, size_t* unused_areas_count);

  void OnReceiverDisconnected();

  void ShutDown();

  // Passed in by the StorageServiceImpl that owns this object. Used to signal
  // that this SessionStorageImpl can be destructed when the Receiver is
  // disconnected.
  DestructSessionStorageCallback destruct_callback_;
  // Since the session storage object hierarchy references iterators owned by
  // the metadata, make sure it is destroyed last on destruction.
  SessionStorageMetadata metadata_;

  BackingMode backing_mode_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
  } connection_state_ = NO_CONNECTION;

  // The profile data directory, which is an ancestor of the database path.
  // Empty for in-memory databases. When not empty, the owner of
  // `SessionStorageImpl` uses this path as an ID for the `SessionStorageImpl`
  // instance.
  const base::FilePath storage_partition_directory_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  mojo::Receiver<mojom::SessionStorageControl> receiver_;

  // `database_` is null after failing to open repeatedly.
  std::unique_ptr<AsyncDomStorageDatabase> database_;
  // This can be true even if the profile is not in-memory, since we attempt
  // to create an in-memory DB if on-disk fails. This variable has no meaning
  // if `database_` is null.
  bool in_memory_ = false;
  bool tried_to_recreate_during_open_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // The removal of items from this map is managed by the refcounting in
  // SessionStorageDataMap.
  // Populated after the database is connected.
  std::map</*map_id=*/int64_t, raw_ptr<SessionStorageDataMap, CtnExperimental>>
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

  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  base::WeakPtrFactory<SessionStorageImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_IMPL_H_
