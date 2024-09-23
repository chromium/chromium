// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/debug_info_event_listener.h"
#include "components/sync/engine/events/protocol_event_buffer.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/nudge_handler.h"
#include "components/sync/engine/sync_engine_event_listener.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_status_tracker.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace syncer {

class Cryptographer;
class DataTypeRegistry;
class SyncCycleContext;

// Lives on the sync sequence.
class SyncManagerImpl
    : public SyncManager,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public SyncEncryptionHandler::Observer,
      public NudgeHandler {
 public:
  // Create an uninitialized SyncManager.  Callers must Init() before using.
  // |network_connection_tracker| must not be null and must outlive this object.
  SyncManagerImpl(
      const std::string& name,
      network::NetworkConnectionTracker* network_connection_tracker);

  SyncManagerImpl(const SyncManagerImpl&) = delete;
  SyncManagerImpl& operator=(const SyncManagerImpl&) = delete;

  ~SyncManagerImpl() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  DataTypeSet InitialSyncEndedTypes() override;
  DataTypeSet GetConnectedTypes() override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(ConfigureReason reason,
                       DataTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       base::OnceClosure ready_task) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void OnIncomingInvalidation(
      DataType type,
      std::unique_ptr<SyncInvalidation> invalidation) override;
  void AddObserver(SyncManager::Observer* observer) override;
  void RemoveObserver(SyncManager::Observer* observer) override;
  void ShutdownOnSyncThread() override;
  DataTypeConnector* GetDataTypeConnector() override;
  std::unique_ptr<DataTypeConnector> GetDataTypeConnectorProxy() override;
  std::string cache_guid() override;
  std::string birthday() override;
  std::string bag_of_chips() override;
  bool HasUnsyncedItemsForTest() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents()
      override;
  void OnCookieJarChanged(bool account_mismatch) override;
  void UpdateActiveDevicesInvalidationInfo(
      ActiveDevicesInvalidationInfo active_devices_invalidation_info) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableProtocolError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(DataTypeSet throttled_types) override;
  void OnBackedOffTypesChanged(DataTypeSet backed_off_types) override;
  void OnMigrationRequested(DataTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  // ServerConnectionEventListener implementation.
  void OnServerConnectionEvent(const ServerConnectionEvent& event) override;

  // Handle explicit requests to fetch updates for the given types.
  void RefreshTypes(DataTypeSet types) override;

  // NetworkConnectionTracker::NetworkConnectionObserver implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // NudgeHandler implementation.
  void NudgeForInitialDownload(DataType type) override;
  void NudgeForCommit(DataType type) override;
  void SetHasPendingInvalidations(DataType type,
                                  bool has_pending_invalidations) override;

 private:
  void NotifySyncStatusChanged(const SyncStatus& status);

  const std::string name_;

  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<SyncManager::Observer>::UncheckedAndDanglingUntriaged
      observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  std::unique_ptr<ServerConnectionManager> connection_manager_;

  // Maintains state that affects the way we interact with different sync types.
  // This state changes when entering or exiting a configuration cycle.
  std::unique_ptr<DataTypeRegistry> data_type_registry_;

  // A container of various bits of information used by the SyncScheduler to
  // create SyncCycles.  Must outlive the SyncScheduler.
  std::unique_ptr<SyncCycleContext> cycle_context_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  std::unique_ptr<SyncScheduler> scheduler_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components. Initialized in Init().
  std::unique_ptr<SyncStatusTracker> sync_status_tracker_;

  // Set to true once Init has been called.
  bool initialized_ = false;

  bool observing_network_connectivity_changes_ = false;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  ProtocolEventBuffer protocol_event_buffer_;

  raw_ptr<SyncEncryptionHandler> sync_encryption_handler_ = nullptr;

  std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy_;

  base::WeakPtrFactory<SyncManagerImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_IMPL_H_
