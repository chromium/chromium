// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class ActiveDevicesProvider;
class ModelTypeConnector;
class ProtocolEvent;
class SyncEngineBackend;
class SyncInvalidationsService;
struct SyncProtocolError;
class SyncTransportDataPrefs;

// The only real implementation of the SyncEngine. See that interface's
// definition for documentation of public methods.
class SyncEngineImpl : public SyncEngine,
                       public invalidation::InvalidationHandler,
                       public InvalidationsListener,
                       public FCMRegistrationTokenObserver {
 public:
  using Status = SyncStatus;

  SyncEngineImpl(const std::string& name,
                 invalidation::InvalidationService* invalidator,
                 SyncInvalidationsService* sync_invalidations_service,
                 std::unique_ptr<ActiveDevicesProvider> active_devices_provider,
                 std::unique_ptr<SyncTransportDataPrefs> prefs,
                 const base::FilePath& sync_data_folder,
                 scoped_refptr<base::SequencedTaskRunner> sync_task_runner,
                 const base::RepeatingClosure& sync_transport_data_cleared_cb);

  SyncEngineImpl(const SyncEngineImpl&) = delete;
  SyncEngineImpl& operator=(const SyncEngineImpl&) = delete;

  ~SyncEngineImpl() override;

  // SyncEngine implementation.
  void Initialize(InitParams params) override;
  bool IsInitialized() const override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  std::string GetCacheGuid() const override;
  std::string GetBirthday() const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  void StartConfiguration() override;
  void StartSyncingWithServer() override;
  void StartHandlingInvalidations() override;
  void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) override;
  void SetExplicitPassphraseDecryptionKey(std::unique_ptr<Nigori> key) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys,
      base::OnceClosure done_cb) override;
  void StopSyncingForShutdown() override;
  void Shutdown(ShutdownReason reason) override;
  void ConfigureDataTypes(ConfigureParams params) override;
  void ConnectDataType(ModelType type,
                       std::unique_ptr<DataTypeActivationResponse>) override;
  void DisconnectDataType(ModelType type) override;
  void SetProxyTabsDatatypeEnabled(bool enabled) override;
  const Status& GetDetailedStatus() const override;
  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(ModelTypeSet)> cb) const override;
  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;
  void OnCookieJarChanged(bool account_mismatch,
                          base::OnceClosure callback) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void GetNigoriNodeForDebugging(AllNodesCallback callback) override;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  void OnInvalidatorClientIdChange(const std::string& client_id) override;

  // InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  // FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

  static std::string GenerateCacheGUIDForTest();

 private:
  friend class SyncEngineBackend;

  // Called when the syncer has finished performing a configuration.
  void FinishConfigureDataTypesOnFrontendLoop(const ModelTypeSet enabled_types,
                                              base::OnceClosure ready_task);

  // Reports backend initialization success.  Includes some objects from sync
  // manager initialization to be passed back to the UI thread.
  //
  // |model_type_connector| is our ModelTypeConnector, which is owned because in
  // production it is a proxy object to the real ModelTypeConnector.
  void HandleInitializationSuccessOnFrontendLoop(
      std::unique_ptr<ModelTypeConnector> model_type_connector,
      const std::string& birthday,
      const std::string& bag_of_chips);

  // Forwards a ProtocolEvent to the host. Will not be called unless a call to
  // SetForwardProtocolEvents() explicitly requested that we start forwarding
  // these events.
  void HandleProtocolEventOnFrontendLoop(std::unique_ptr<ProtocolEvent> event);

  // Overwrites the kSyncInvalidationVersions preference with the most recent
  // set of invalidation versions for each type.
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  void HandleSyncStatusChanged(const SyncStatus& status);

  // Handles backend initialization failure.
  void HandleInitializationFailureOnFrontendLoop();

  // Called from SyncEngineBackend::OnSyncCycleCompleted to handle updating
  // frontend thread components.
  void HandleSyncCycleCompletedOnFrontendLoop(
      const SyncCycleSnapshot& snapshot);

  // Let the front end handle the actionable error event.
  void HandleActionableErrorEventOnFrontendLoop(
      const SyncProtocolError& sync_error);

  // Handle a migration request.
  void HandleMigrationRequestedOnFrontendLoop(const ModelTypeSet types);

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(ConnectionStatus status);

  void OnCookieJarChangedDoneOnFrontendLoop(base::OnceClosure callback);

  void SendInterestedTopicsToInvalidator();

  // Called on each device infos change and might be called more than once with
  // the same |active_devices|.
  void OnActiveDevicesChanged();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Helper function that clears SyncTransportDataPrefs and also notifies
  // upper layers via |sync_transport_data_cleared_cb_|.
  void ClearLocalTransportDataAndNotify();

  // Updates the current state of standalone invalidations.
  void UpdateStandaloneInvalidationsState();

  // The task runner where all the sync engine operations happen.
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  const std::unique_ptr<SyncTransportDataPrefs> prefs_;

  const base::RepeatingClosure sync_transport_data_cleared_cb_;

  // Our backend, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |backend_| is created on UI loop but released on
  // sync loop.
  scoped_refptr<SyncEngineBackend> backend_;

  // A handle referencing the main interface for sync data types. This
  // object is owned because in production code it is a proxy object.
  std::unique_ptr<ModelTypeConnector> model_type_connector_;

  bool initialized_ = false;

  // The host which we serve (and are owned by). Set in Initialize() and nulled
  // out in StopSyncingForShutdown().
  raw_ptr<SyncEngineHost> host_ = nullptr;

  raw_ptr<invalidation::InvalidationService> invalidator_ = nullptr;
  bool invalidation_handler_registered_ = false;

  // Sync invalidation service, it may be nullptr if sync invalidations are
  // disabled or not supported. It doesn't need to have the same as
  // |invalidation_handler_registered_| flag as the service doesn't have topics
  // to unsibscribe.
  raw_ptr<SyncInvalidationsService> sync_invalidations_service_ = nullptr;

  ModelTypeSet last_enabled_types_;
  bool sessions_invalidation_enabled_;

  SyncStatus cached_status_;

  std::unique_ptr<ActiveDevicesProvider> active_devices_provider_;

  // Checks that we're on the same thread this was constructed on (UI thread).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncEngineImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_IMPL_H_
