// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_SYNC_MANAGER_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_SYNC_MANAGER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine_impl/all_status.h"
#include "components/sync/engine_impl/debug_info_event_listener.h"
#include "components/sync/engine_impl/events/protocol_event_buffer.h"
#include "components/sync/engine_impl/js_mutation_event_observer.h"
#include "components/sync/engine_impl/js_sync_encryption_handler_observer.h"
#include "components/sync/engine_impl/js_sync_manager_observer.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/engine_impl/nudge_handler.h"
#include "components/sync/engine_impl/sync_engine_event_listener.h"
#include "components/sync/js/js_backend.h"
#include "components/sync/syncable/change_reorder_buffer.h"
#include "components/sync/syncable/directory_change_delegate.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace syncer {

class Cryptographer;
class ModelTypeRegistry;
class SyncCycleContext;
class TypeDebugInfoObserver;

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SyncManagerImpl
    : public SyncManager,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public JsBackend,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public syncable::DirectoryChangeDelegate,
      public SyncEncryptionHandler::Observer,
      public NudgeHandler {
 public:
  // Create an uninitialized SyncManager.  Callers must Init() before using.
  // |network_connection_tracker| must not be null and must outlive this object.
  SyncManagerImpl(
      const std::string& name,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~SyncManagerImpl() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  ModelTypeSet InitialSyncEndedTypes() override;
  ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) override;
  void PurgePartiallySyncedTypes() override;
  void PurgeDisabledTypes(ModelTypeSet to_purge,
                          ModelTypeSet to_journal,
                          ModelTypeSet to_unapply) override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(ConfigureReason reason,
                       ModelTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       const base::Closure& ready_task) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void OnIncomingInvalidation(
      ModelType type,
      std::unique_ptr<InvalidationInterface> invalidation) override;
  void AddObserver(SyncManager::Observer* observer) override;
  void RemoveObserver(SyncManager::Observer* observer) override;
  SyncStatus GetDetailedStatus() const override;
  void SaveChanges() override;
  void ShutdownOnSyncThread() override;
  UserShare* GetUserShare() override;
  ModelTypeConnector* GetModelTypeConnector() override;
  std::unique_ptr<ModelTypeConnector> GetModelTypeConnectorProxy() override;
  std::string cache_guid() override;
  std::string birthday() override;
  std::string bag_of_chips() override;
  bool HasUnsyncedItemsForTest() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents()
      override;
  void RegisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;
  void OnCookieJarChanged(bool account_mismatch, bool empty_jar) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  void UpdateInvalidationClientId(const std::string& client_id) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(ModelTypeSet throttled_types) override;
  void OnBackedOffTypesChanged(ModelTypeSet backed_off_types) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  // ServerConnectionEventListener implementation.
  void OnServerConnectionEvent(const ServerConnectionEvent& event) override;

  // JsBackend implementation.
  void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) override;

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) override;
  ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) override;
  void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64_t>* entries_changed) override;
  void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64_t>* entries_changed) override;

  // Handle explicit requests to fetch updates for the given types.
  void RefreshTypes(ModelTypeSet types) override;

  // NetworkConnectionTracker::NetworkConnectionObserver implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // NudgeHandler implementation.
  void NudgeForInitialDownload(ModelType type) override;
  void NudgeForCommit(ModelType type) override;

  static std::string GenerateCacheGUIDForTest();

 protected:
  // Helper functions.  Virtual for testing.
  virtual void NotifyInitializationSuccess();
  virtual void NotifyInitializationFailure();

 private:
  friend class SyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, NudgeDelayTest);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, PurgeDisabledTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, PurgeUnappliedTypes);

  struct NotificationInfo {
    NotificationInfo();
    ~NotificationInfo();

    int total_count;
    std::string payload;

    // Returned pointer owned by the caller.
    base::DictionaryValue* ToValue() const;
  };

  using NotificationInfoMap = std::map<ModelType, NotificationInfo>;

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry.  Note that a node's index may change without its
  // UNIQUE_POSITION changing if its sibling nodes were changed.  To handle such
  // cases, we rely on the caller to treat a position update on any sibling as
  // updating the positions of all siblings.
  bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) const;

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  bool VisiblePropertiesDiffer(const syncable::EntryKernelMutation& mutation,
                               const Cryptographer* cryptographer) const;

  // Opens the directory.
  bool OpenDirectory(InitArgs* args);

  void RequestNudgeForDataTypes(const base::Location& nudge_location,
                                ModelTypeSet type);

  // If this is a deletion for a password, sets the legacy
  // ExtraPasswordChangeRecordData field of |buffer|. Otherwise sets
  // |buffer|'s specifics field to contain the unencrypted data.
  void SetExtraChangeRecordData(int64_t id,
                                ModelType type,
                                ChangeReorderBuffer* buffer,
                                const Cryptographer* cryptographer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  syncable::Directory* directory();

  base::FilePath database_path_;

  const std::string name_;

  network::NetworkConnectionTracker* network_connection_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Thread-safe handle used by
  // HandleCalculateChangesChangeEventFromSyncApi(), which can be
  // called from any thread.  Valid only between between calls to
  // Init() and Shutdown().
  //
  // TODO(akalin): Ideally, we wouldn't need to store this; instead,
  // we'd have another worker class which implements
  // HandleCalculateChangesChangeEventFromSyncApi() and we'd pass it a
  // WeakHandle when we construct it.
  WeakHandle<SyncManagerImpl> weak_handle_this_;

  // We give a handle to share_ to clients of the API for use when constructing
  // any transaction type.
  UserShare* share_;

  // This can be called from any thread, but only between calls to
  // OpenDirectory() and ShutdownOnSyncThread().
  WeakHandle<SyncManager::ChangeObserver> change_observer_;

  base::ObserverList<SyncManager::Observer>::Unchecked observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  std::unique_ptr<ServerConnectionManager> connection_manager_;

  // Maintains state that affects the way we interact with different sync types.
  // This state changes when entering or exiting a configuration cycle.
  std::unique_ptr<ModelTypeRegistry> model_type_registry_;

  // A container of various bits of information used by the SyncScheduler to
  // create SyncCycles.  Must outlive the SyncScheduler.
  std::unique_ptr<SyncCycleContext> cycle_context_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  std::unique_ptr<SyncScheduler> scheduler_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this map is a store of change records produced by
  // HandleChangeEventFromSyncer during the CALCULATE_CHANGES step. The changes
  // are grouped by model type, and are stored here in tree order to be
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING step
  // by HandleTransactionEndingChangeEvent. The list is cleared after observer
  // finishes processing.
  using ChangeRecordMap = std::map<int, ImmutableChangeRecordList>;
  ChangeRecordMap change_records_;

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  bool observing_network_connectivity_changes_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsSyncManagerObserver js_sync_manager_observer_;
  JsMutationEventObserver js_mutation_event_observer_;
  JsSyncEncryptionHandlerObserver js_sync_encryption_handler_observer_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  ProtocolEventBuffer protocol_event_buffer_;

  base::Closure report_unrecoverable_error_function_;

  SyncEncryptionHandler* sync_encryption_handler_;

  std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy_;

  base::WeakPtrFactory<SyncManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNC_MANAGER_IMPL_H_
