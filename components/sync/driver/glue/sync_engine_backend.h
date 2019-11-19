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
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/invalidation.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/system_encryptor.h"
#include "components/sync/driver/glue/sync_engine_impl.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/syncable/user_share.h"
#include "url/gurl.h"

namespace syncer {

class ModelTypeController;
class SyncEngineImpl;

namespace syncable {

class NigoriHandlerProxy;

}  // namespace syncable

class SyncEngineBackend : public base::RefCountedThreadSafe<SyncEngineBackend>,
                          public base::trace_event::MemoryDumpProvider,
                          public SyncManager::Observer,
                          public TypeDebugInfoObserver {
 public:
  using AllNodesCallback =
      base::OnceCallback<void(const ModelType,
                              std::unique_ptr<base::ListValue>)>;

  SyncEngineBackend(const std::string& name,
                    const base::FilePath& sync_data_folder,
                    const base::WeakPtr<SyncEngineImpl>& host);

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

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

  // TypeDebugInfoObserver implementation
  void OnCommitCountersUpdated(ModelType type,
                               const CommitCounters& counters) override;
  void OnUpdateCountersUpdated(ModelType type,
                               const UpdateCounters& counters) override;
  void OnStatusCountersUpdated(ModelType type,
                               const StatusCounters& counters) override;

  // Forwards an invalidation state change to the sync manager.
  void DoOnInvalidatorStateChange(InvalidatorState state);

  // Forwards an invalidation to the sync manager.
  void DoOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map);

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
  void DoAddTrustedVaultDecryptionKeys(const std::vector<std::string>& keys);

  // Called to turn on encryption of all sync data as well as
  // reencrypt everything.
  void DoEnableEncryptEverything();

  // Ask the syncer to check for updates for the specified types.
  void DoRefreshTypes(ModelTypeSet types);

  // Called to perform tasks which require the control data to be downloaded.
  // This includes refreshing encryption, etc.
  void DoInitialProcessControlTypes();

  // The shutdown order is a bit complicated:
  // 1) Call ShutdownOnUIThread() from |frontend_loop_| to request sync manager
  //    to stop as soon as possible.
  // 2) Post DoShutdown() to sync loop to clean up backend state, save
  //    directory and destroy sync manager.
  void ShutdownOnUIThread();
  void DoShutdown(ShutdownReason reason);
  void DoDestroySyncManager();

  // Configuration methods that must execute on sync loop.
  void DoPurgeDisabledTypes(const ModelTypeSet& to_purge,
                            const ModelTypeSet& to_journal,
                            const ModelTypeSet& to_unapply);
  void DoConfigureSyncer(ModelTypeConfigurer::ConfigureParams params);
  void DoFinishConfigureDataTypes(
      ModelTypeSet types_to_config,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task);

  // Set the base request context to use when making HTTP calls.
  // This method will add a reference to the context to persist it
  // on the IO thread. Must be removed from IO thread.

  SyncManager* sync_manager() { return sync_manager_.get(); }

  void SendBufferedProtocolEventsAndEnableForwarding();
  void DisableProtocolEventForwarding();

  // Enables the forwarding of directory type debug counters to the
  // SyncEngineHost. Also requests that updates to all counters be emitted right
  // away to initialize any new listeners' states.
  void EnableDirectoryTypeDebugInfoForwarding();

  // Disables forwarding of directory type debug counters.
  void DisableDirectoryTypeDebugInfoForwarding();

  // Tell the sync manager to persist its state by writing to disk.
  // Called on the sync thread, both by a timer and, on Android, when the
  // application is backgrounded.
  void SaveChanges();

  // Notify the syncer that the cookie jar has changed.
  void DoOnCookieJarChanged(bool account_mismatch,
                            bool empty_jar,
                            const base::Closure& callback);

  // Notify about change in client id.
  void DoOnInvalidatorClientIdChange(const std::string& client_id);

  // Returns a ListValue representing Nigori node.
  void GetNigoriNodeForDebugging(AllNodesCallback callback);

  bool HasUnsyncedItemsForTest() const;

 private:
  friend class base::RefCountedThreadSafe<SyncEngineBackend>;

  ~SyncEngineBackend() override;

  // For the olg tango based invalidations method returns true if the
  // invalidation has version lower than last seen version for this datatype.
  bool ShouldIgnoreRedundantInvalidation(const Invalidation& invalidation,
                                         ModelType Type);

  // Invoked when initialization of syncapi is complete and we can start
  // our timer.
  // This must be called from the thread on which SaveChanges is intended to
  // be run on; the host's |registrar_->sync_thread()|.
  void StartSavingChanges();

  void LoadAndConnectNigoriController();

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const base::FilePath sync_data_folder_;

  // Our parent SyncEngineImpl.
  WeakHandle<SyncEngineImpl> host_;

  // Non-null only between calls to DoInitialize() and DoShutdown().
  std::unique_ptr<SyncBackendRegistrar> registrar_;

  // The timer used to periodically call SaveChanges.
  std::unique_ptr<base::RepeatingTimer> save_changes_timer_;

  // Our encryptor, which uses Chrome's encryption functions.
  SystemEncryptor encryptor_;

  // We hold |user_share_| here as a dependency for |sync_encryption_handler_|.
  // Should outlive |sync_encryption_handler_| and |sync_manager_|.
  UserShare user_share_;

  // Points to either SyncEncryptionHandlerImpl or NigoriSyncBridgeImpl
  // depending on whether USS implementation of Nigori is enabled or not.
  // Should outlive |sync_manager_|.
  std::unique_ptr<SyncEncryptionHandler> sync_encryption_handler_;

  std::unique_ptr<syncable::NigoriHandlerProxy> nigori_handler_proxy_;

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

  // Set when the forwarding of per-type debug counters is enabled.
  bool forward_type_info_ = false;

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
