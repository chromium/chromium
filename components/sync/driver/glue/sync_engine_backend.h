// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/invalidation.h"
#include "components/sync/base/system_encryptor.h"
#include "components/sync/driver/glue/sync_engine_impl.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_status_observer.h"
#include "components/sync/engine_impl/cancelation_signal.h"
#include "url/gurl.h"

namespace syncer {

class ModelTypeController;
class SyncEngineImpl;

class SyncEngineBackend : public base::RefCountedThreadSafe<SyncEngineBackend>,
                          public SyncManager::Observer,
                          public SyncStatusObserver {
 public:
  using AllNodesCallback =
      base::OnceCallback<void(const ModelType,
                              std::unique_ptr<base::ListValue>)>;

  SyncEngineBackend(const std::string& name,
                    const base::FilePath& sync_data_folder,
                    const base::WeakPtr<SyncEngineImpl>& host);

  // SyncManager::Observer implementation.  The Core just acts like an air
  // traffic controller here, forwarding incoming messages to appropriate
  // landing threads.
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      bool success) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnActionableError(const SyncProtocolError& sync_error) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  // SyncStatusObserver implementation.
  void OnSyncStatusChanged(const SyncStatus& status) override;

  // Forwards an invalidation state change to the sync manager.
  void DoOnInvalidatorStateChange(InvalidatorState state);

  // Forwards an invalidation to the sync manager.
  void DoOnIncomingInvalidation(const TopicInvalidationMap& invalidation_map);

  // Note:
  //
  // The Do* methods are the various entry points from our SyncEngineImpl.
  // They are all called on the sync thread to actually perform synchronous (and
  // potentially blocking) syncapi operations.
  //
  // Called to perform initialization of the syncapi on behalf of
  // SyncEngine::Initialize.
  void DoInitialize(SyncEngine::InitParams params);

  // Called to perform credential update on behalf of
  // SyncEngine::UpdateCredentials.
  void DoUpdateCredentials(const SyncCredentials& credentials);

  // Called to invalidate the credentials on behalf of
  // SyncEngine::InvalidateCredentials.
  void DoInvalidateCredentials();

  // Switches sync engine into configuration mode. In this mode only initial
  // data for newly enabled types is downloaded from server. No local changes
  // are committed to server.
  void DoStartConfiguration();

  // Called to tell the syncapi to start syncing (generally after
  // initialization and authentication).
  void DoStartSyncing(base::Time last_poll_time);

  // Called to set the passphrase for encryption.
  void DoSetEncryptionPassphrase(const std::string& passphrase);

  // Called to decrypt the pending keys using user-entered passphrases.
  void DoSetDecryptionPassphrase(const std::string& passphrase);

  // Called to decrypt the pending keys using trusted vault keys.
  void DoAddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys);

  // Ask the syncer to check for updates for the specified types.
  void DoRefreshTypes(ModelTypeSet types);

  // Called to perform tasks which require the control data to be downloaded.
  // This includes refreshing encryption, etc.
  void DoInitialProcessControlTypes();

  // The shutdown order is a bit complicated:
  // 1) Call ShutdownOnUIThread() from |frontend_loop_| to request sync manager
  //    to stop as soon as possible.
  // 2) Post DoShutdown() to sync loop to clean up backend state and destroy
  //    sync manager.
  void ShutdownOnUIThread();
  void DoShutdown(ShutdownReason reason);
  void DoDestroySyncManager();

  // Configuration methods that must execute on sync loop.
  void DoPurgeDisabledTypes(const ModelTypeSet& to_purge);
  void DoConfigureSyncer(ModelTypeConfigurer::ConfigureParams params);
  void DoFinishConfigureDataTypes(
      ModelTypeSet types_to_config,
      base::OnceCallback<void(ModelTypeSet, ModelTypeSet)> ready_task);

  // Set the base request context to use when making HTTP calls.
  // This method will add a reference to the context to persist it
  // on the IO thread. Must be removed from IO thread.

  SyncManager* sync_manager() { return sync_manager_.get(); }

  void SendBufferedProtocolEventsAndEnableForwarding();
  void DisableProtocolEventForwarding();

  // Notify the syncer that the cookie jar has changed.
  void DoOnCookieJarChanged(bool account_mismatch,
                            base::OnceClosure callback);

  // Notify about change in client id.
  void DoOnInvalidatorClientIdChange(const std::string& client_id);

  // Forwards an invalidation to the sync manager for all data types from the
  // |payload|.
  void DoOnInvalidationReceived(const std::string& payload);

  // Returns a ListValue representing Nigori node.
  void GetNigoriNodeForDebugging(AllNodesCallback callback);

  bool HasUnsyncedItemsForTest() const;

  // Called on each device infos change and might be called more than once with
  // the same |active_devices|.
  void DoOnActiveDevicesChanged(size_t active_devices);

 private:
  friend class base::RefCountedThreadSafe<SyncEngineBackend>;

  ~SyncEngineBackend() override;

  // For the olg tango based invalidations method returns true if the
  // invalidation has version lower than last seen version for this datatype.
  bool ShouldIgnoreRedundantInvalidation(const Invalidation& invalidation,
                                         ModelType Type);

  void LoadAndConnectNigoriController();

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const base::FilePath sync_data_folder_;

  // Our parent SyncEngineImpl.
  WeakHandle<SyncEngineImpl> host_;

  // Our encryptor, which uses Chrome's encryption functions.
  SystemEncryptor encryptor_;

  // Should outlive |sync_manager_|.
  std::unique_ptr<SyncEncryptionHandler> sync_encryption_handler_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  std::unique_ptr<SyncManager> sync_manager_;

  // Required for |nigori_controller_| LoadModels().
  CoreAccountId authenticated_account_id_;

  // Initialized in OnInitializationComplete() iff USS implementation of Nigori
  // is enabled.
  std::unique_ptr<ModelTypeController> nigori_controller_;

  // Temporary holder of sync manager's initialization results. Set by
  // OnInitializeComplete, and consumed when we pass it via OnEngineInitialized
  // in the final state of HandleInitializationSuccessOnFrontendLoop.
  WeakHandle<JsBackend> js_backend_;
  WeakHandle<DataTypeDebugInfoListener> debug_info_listener_;

  // This signal allows us to send requests to shut down the
  // ServerConnectionManager without having to wait for it to finish
  // initializing first.
  //
  // See comment in ShutdownOnUIThread() for more details.
  CancelationSignal stop_syncing_signal_;

  // Set when we've been asked to forward sync protocol events to the frontend.
  bool forward_protocol_events_ = false;

  // A map of data type -> invalidation version to track the most recently
  // received invalidation version for each type.
  // This allows dropping any invalidations with versions older than those
  // most recently received for that data type.
  std::map<ModelType, int64_t> last_invalidation_versions_;

  // Checks that we are on the sync thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncEngineBackend> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncEngineBackend);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_
