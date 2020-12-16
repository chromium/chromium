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
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine_impl/all_status.h"
#include "components/sync/engine_impl/debug_info_event_listener.h"
#include "components/sync/engine_impl/events/protocol_event_buffer.h"
#include "components/sync/engine_impl/js_sync_encryption_handler_observer.h"
#include "components/sync/engine_impl/js_sync_manager_observer.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/engine_impl/nudge_handler.h"
#include "components/sync/engine_impl/sync_engine_event_listener.h"
#include "components/sync/js/js_backend.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace syncer {

class Cryptographer;
class ModelTypeRegistry;
class SyncCycleContext;

// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SyncManagerImpl
    : public SyncManager,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public JsBackend,
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
  ~SyncManagerImpl() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  ModelTypeSet InitialSyncEndedTypes() override;
  ModelTypeSet GetEnabledTypes() override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(ConfigureReason reason,
                       ModelTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       base::OnceClosure ready_task) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void OnIncomingInvalidation(
      ModelType type,
      std::unique_ptr<InvalidationInterface> invalidation) override;
  void AddObserver(SyncManager::Observer* observer) override;
  void RemoveObserver(SyncManager::Observer* observer) override;
  void ShutdownOnSyncThread() override;
  ModelTypeConnector* GetModelTypeConnector() override;
  std::unique_ptr<ModelTypeConnector> GetModelTypeConnectorProxy() override;
  std::string cache_guid() override;
  std::string birthday() override;
  std::string bag_of_chips() override;
  bool HasUnsyncedItemsForTest() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<ProtocolEvent>> GetBufferedProtocolEvents()
      override;
  void OnCookieJarChanged(bool account_mismatch) override;
  void UpdateInvalidationClientId(const std::string& client_id) override;
  void UpdateSingleClientStatus(bool single_client) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
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

  // Handle explicit requests to fetch updates for the given types.
  void RefreshTypes(ModelTypeSet types) override;

  // NetworkConnectionTracker::NetworkConnectionObserver implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // NudgeHandler implementation.
  void NudgeForInitialDownload(ModelType type) override;
  void NudgeForCommit(ModelType type) override;

 protected:
  // Helper functions.  Virtual for testing.
  virtual void NotifyInitializationSuccess();
  virtual void NotifyInitializationFailure();

 private:
  friend class SyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, NudgeDelayTest);

  struct NotificationInfo {
    NotificationInfo();
    ~NotificationInfo();

    int total_count;
    std::string payload;

    // Returned pointer owned by the caller.
    base::DictionaryValue* ToValue() const;
  };

  using NotificationInfoMap = std::map<ModelType, NotificationInfo>;

  void RequestNudgeForDataTypes(const base::Location& nudge_location,
                                ModelTypeSet type);

  const std::string name_;

  network::NetworkConnectionTracker* network_connection_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

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

  // Set to true once Init has been called.
  bool initialized_;

  bool observing_network_connectivity_changes_;

  // Map used to store the notification info to be displayed in
  // chrome://sync-internals page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsSyncManagerObserver js_sync_manager_observer_;
  JsSyncEncryptionHandlerObserver js_sync_encryption_handler_observer_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  ProtocolEventBuffer protocol_event_buffer_;

  SyncEncryptionHandler* sync_encryption_handler_;

  std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy_;

  base::WeakPtrFactory<SyncManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNC_MANAGER_IMPL_H_
