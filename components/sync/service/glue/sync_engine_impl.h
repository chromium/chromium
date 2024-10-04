// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_IMPL_H_
#define COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/data_type_configurer.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"

namespace syncer {

class ActiveDevicesProvider;
class DataTypeConnector;
class ProtocolEvent;
class SyncEngineBackend;
class SyncInvalidationsService;
struct SyncProtocolError;
class SyncTransportDataPrefs;

// The only real implementation of the SyncEngine. See that interface's
// definition for documentation of public methods.
// Lives on the UI thread, and handles task-posting to SyncEngineBackend on
// the sync sequence as necessary.
class SyncEngineImpl : public SyncEngine,
                       public InvalidationsListener,
                       public FCMRegistrationTokenObserver {
 public:
  // |sync_invalidations_service| must not be null.
  SyncEngineImpl(const std::string& name,
                 SyncInvalidationsService* sync_invalidations_service,
                 std::unique_ptr<ActiveDevicesProvider> active_devices_provider,
                 std::unique_ptr<SyncTransportDataPrefs> prefs,
                 const base::FilePath& sync_data_folder,
                 scoped_refptr<base::SequencedTaskRunner> sync_task_runner);

  SyncEngineImpl(const SyncEngineImpl&) = delete;
  SyncEngineImpl& operator=(const SyncEngineImpl&) = delete;

  ~SyncEngineImpl() override;

  // SyncEngine implementation.
  void Initialize(InitParams params) override;
  bool IsInitialized() const override;
  void TriggerRefresh(const DataTypeSet& types) override;
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
  void ConnectDataType(DataType type,
                       std::unique_ptr<DataTypeActivationResponse>) override;
  void DisconnectDataType(DataType type) override;
  const SyncStatus& GetDetailedStatus() const override;
  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(DataTypeSet)> cb) const override;
  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;
  void OnCookieJarChanged(bool account_mismatch,
                          base::OnceClosure callback) override;
  bool IsNextPollTimeInThePast() const override;
  void ClearNigoriDataForMigration() override;
  void GetNigoriNodeForDebugging(AllNodesCallback callback) override;
  void RecordNigoriMemoryUsageAndCountsHistograms() override;

  // InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  // FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

  static std::string GenerateCacheGUIDForTest();

 private:
  friend class SyncEngineBackend;

  // Called when the syncer has finished performing a configuration.
  void FinishConfigureDataTypesOnFrontendLoop(const DataTypeSet enabled_types,
                                              base::OnceClosure ready_task);

  // Reports backend initialization success.  Includes some objects from sync
  // manager initialization to be passed back to the UI thread.
  //
  // |data_type_connector| is our DataTypeConnector, which is owned because in
  // production it is a proxy object to the real DataTypeConnector.
  void HandleInitializationSuccessOnFrontendLoop(
      std::unique_ptr<DataTypeConnector> data_type_connector,
      const std::string& birthday,
      const std::string& bag_of_chips);

  // Forwards a ProtocolEvent to the host. Will not be called unless a call to
  // SetForwardProtocolEvents() explicitly requested that we start forwarding
  // these events.
  void HandleProtocolEventOnFrontendLoop(std::unique_ptr<ProtocolEvent> event);

  void HandleSyncStatusChanged(const SyncStatus& status);

  // Handles backend initialization failure.
  void HandleInitializationFailureOnFrontendLoop();

  // Called from SyncEngineBackend::OnSyncCycleCompleted to handle updating
  // frontend sequence components.
  void HandleSyncCycleCompletedOnFrontendLoop(
      const SyncCycleSnapshot& snapshot);

  // Let the front end handle the actionable error event.
  void HandleActionableProtocolErrorEventOnFrontendLoop(
      const SyncProtocolError& sync_error);

  // Handle a migration request.
  void HandleMigrationRequestedOnFrontendLoop(const DataTypeSet types);

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(ConnectionStatus status);

  void OnCookieJarChangedDoneOnFrontendLoop(base::OnceClosure callback);

  // Called on each device infos change and might be called more than once with
  // the same |active_devices|.
  void OnActiveDevicesChanged();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Updates the current state of standalone invalidations. Note that the
  // invalidations can be handled even if the invalidation service is not fully
  // initialized yet (e.g. while processing the incoming queue of messages
  // received during browser startup).
  void UpdateStandaloneInvalidationsState();

  // Updates invalidator's state.
  void OnInvalidatorStateChange(bool enabled);

  // The task runner where all the sync engine operations happen.
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  const std::unique_ptr<SyncTransportDataPrefs> prefs_;

  // The cache GUID and birthday are stored in prefs, but also cached in memory.
  // This is because in some cases (when an account gets removed from the
  // device), the prefs can get cleared before the SyncEngine is destroyed.
  std::string cached_cache_guid_;
  std::string cached_birthday_;

  raw_ptr<SyncInvalidationsService> sync_invalidations_service_ = nullptr;

  // Our backend, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |backend_| is created on the UI thread but released
  // on the sync sequence.
  scoped_refptr<SyncEngineBackend> backend_;

  // The host which we serve (and are owned by). Set in Initialize() and nulled
  // out in StopSyncingForShutdown().
  raw_ptr<SyncEngineHost> host_ = nullptr;

  bool initialized_ = false;

  // A handle referencing the main interface for sync data types. This
  // object is owned because in production code it is a proxy object.
  std::unique_ptr<DataTypeConnector> data_type_connector_;

  DataTypeSet last_enabled_types_;

  SyncStatus cached_status_;

  std::unique_ptr<ActiveDevicesProvider> active_devices_provider_;

  // Time when current object has been created. Used for metrics only.
  const base::TimeTicks engine_created_time_for_metrics_;

  // Whether the histogram for enabled invalidations has been already recorded.
  // This is used to record only the first "invalidatons enabled" event,
  // otherwise the metrics would be skewed by invalidations disabling and
  // re-enabling.
  bool invalidations_enabled_reported_ = false;

  // Checks that we're on the same sequence this was constructed on (UI
  // sequence).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncEngineImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_IMPL_H_
