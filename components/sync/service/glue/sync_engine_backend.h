// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_BACKEND_H_
#define COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_BACKEND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/data_type_configurer.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace syncer {

class KeyDerivationParams;
class DataTypeController;
class Nigori;
class SyncEngineImpl;

class SyncEngineBackend : public base::RefCountedThreadSafe<SyncEngineBackend>,
                          public SyncManager::Observer {
 public:
  using AllNodesCallback = base::OnceCallback<void(base::Value::List)>;

  // Struct that allows passing back data upon init, for data previously
  // produced by SyncEngineBackend (which doesn't itself have the ability to
  // persist data).
  struct RestoredLocalTransportData {
    RestoredLocalTransportData();
    RestoredLocalTransportData(RestoredLocalTransportData&&);
    RestoredLocalTransportData(const RestoredLocalTransportData&) = delete;
    ~RestoredLocalTransportData();

    // Initial authoritative values (usually read from prefs).
    std::string cache_guid;
    std::string birthday;
    std::string bag_of_chips;

    // Define the polling interval. Must not be zero.
    base::TimeDelta poll_interval;
  };

  // Used to record result of handling of incoming sync invalidations. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  // LINT.IfChange(SyncIncomingInvalidationStatus)
  enum class IncomingInvalidationStatus {
    // The payload parsed successfully and contains at least one valid data
    // type.
    kSuccess = 0,

    // Failed to parse incoming payload, relevant only for sync standalone
    // invalidations.
    kPayloadParseFailed = 1,

    // All data types in the payload are unknown.
    kUnknownDataType = 2,

    kMaxValue = kUnknownDataType,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncIncomingInvalidationStatus)

  SyncEngineBackend(const std::string& name,
                    const base::FilePath& sync_data_folder,
                    const base::WeakPtr<SyncEngineImpl>& host);

  SyncEngineBackend(const SyncEngineBackend&) = delete;
  SyncEngineBackend& operator=(const SyncEngineBackend&) = delete;

  // SyncManager::Observer implementation.  The Backend just acts like an air
  // traffic controller here, forwarding incoming messages to appropriate
  // landing threads.
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnActionableProtocolError(const SyncProtocolError& sync_error) override;
  void OnMigrationRequested(DataTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnSyncStatusChanged(const SyncStatus& status) override;

  // Note:
  //
  // The Do* methods are the various entry points from our SyncEngineImpl.
  // They are all called on the sync thread to actually perform synchronous (and
  // potentially blocking) operations.

  // Forwards an invalidation state change to the sync manager.
  void DoOnInvalidatorStateChange(bool enabled);

  // Called to perform initialization of the syncapi on behalf of
  // SyncEngine::Initialize.
  void DoInitialize(SyncEngine::InitParams params,
                    RestoredLocalTransportData restored_transport_data);

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
  void DoSetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params);

  // Called to decrypt the pending keys using the |key| derived from
  // user-entered passphrase.
  void DoSetExplicitPassphraseDecryptionKey(std::unique_ptr<Nigori> key);

  // Called to decrypt the pending keys using trusted vault keys.
  void DoAddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys);

  // Ask the syncer to check for updates for the specified types.
  void DoRefreshTypes(DataTypeSet types);

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

  // Configuration methods that must execute on sync loop.
  void DoConfigureSyncer(DataTypeConfigurer::ConfigureParams params);
  void DoFinishConfigureDataTypes(
      DataTypeSet types_to_config,
      base::OnceCallback<void(DataTypeSet, DataTypeSet)> ready_task);

  void SendBufferedProtocolEventsAndEnableForwarding();
  void DisableProtocolEventForwarding();

  // Notify the syncer that the cookie jar has changed.
  void DoOnCookieJarChanged(bool account_mismatch, base::OnceClosure callback);

  // Forwards an invalidation to the sync manager for all data types extracted
  // from the |payload|. This method is called for sync standalone
  // invalidations.
  void DoOnStandaloneInvalidationReceived(
      const std::string& payload,
      const DataTypeSet& interested_data_types);

  // Functions to deal with NIGORI, resembling DataTypeController APIs.
  void DoClearNigoriDataForMigration();
  void GetNigoriNodeForDebugging(AllNodesCallback callback);
  void RecordNigoriMemoryUsageAndCountsHistograms();

  bool HasUnsyncedItemsForTest() const;

  // Called on each device infos change and might be called more than once with
  // the same |active_devices|. |fcm_registration_tokens| contains a list of
  // tokens for all known active devices (if available and excluding the local
  // device if reflections are disabled).
  void DoOnActiveDevicesChanged(
      ActiveDevicesInvalidationInfo active_devices_invalidation_info);

 private:
  friend class base::RefCountedThreadSafe<SyncEngineBackend>;

  ~SyncEngineBackend() override;

  void LoadAndConnectNigoriController();

  IncomingInvalidationStatus DoOnStandaloneInvalidationReceivedImpl(
      const std::string& payload,
      const DataTypeSet& interested_data_types);

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const base::FilePath sync_data_folder_;

  // Our parent SyncEngineImpl.
  WeakHandle<SyncEngineImpl> host_;

  // Should outlive |sync_manager_|.
  std::unique_ptr<SyncEncryptionHandler> sync_encryption_handler_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  std::unique_ptr<SyncManager> sync_manager_;

  // Required for |nigori_controller_| LoadModels().
  CoreAccountId authenticated_account_id_;

  // Initialized in Init().
  std::unique_ptr<DataTypeController> nigori_controller_;

  // This signal allows us to send requests to shut down the
  // ServerConnectionManager without having to wait for it to finish
  // initializing first.
  //
  // See comment in ShutdownOnUIThread() for more details.
  CancelationSignal stop_syncing_signal_;

  // Set when we've been asked to forward sync protocol events to the frontend.
  bool forward_protocol_events_ = false;

  // Checks that we are on the sync thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncEngineBackend> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GLUE_SYNC_ENGINE_BACKEND_H_
