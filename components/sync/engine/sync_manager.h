// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/data_type_connector.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/engine/sync_status.h"
#include "url/gurl.h"

namespace syncer {

class CancelationSignal;
class EngineComponentsFactory;
class ExtensionsActivity;
class ProtocolEvent;
class SyncCycleSnapshot;
struct SyncStatus;

// Lives on the sync sequence.
class SyncManager {
 public:
  // An interface the embedding application implements to receive notifications
  // from the SyncManager. Register an observer via SyncManager::AddObserver.
  // All methods are called only on the sync sequence.
  class Observer {
   public:
    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.
    virtual void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) = 0;

    // Called when the status of the connection to the sync server has
    // changed.
    virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

    virtual void OnActionableProtocolError(
        const SyncProtocolError& sync_protocol_error) = 0;

    virtual void OnMigrationRequested(DataTypeSet types) = 0;

    virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

    virtual void OnSyncStatusChanged(const SyncStatus&) = 0;

   protected:
    virtual ~Observer();
  };

  // Arguments for initializing SyncManager.
  struct InitArgs {
    InitArgs();
    ~InitArgs();

    // URL of the sync server.
    GURL service_url;

    // Whether the local backend provided by the LoopbackServer should be used
    // and the location of the local sync backend storage.
    bool enable_local_sync_backend = false;
    base::FilePath local_sync_backend_folder;

    // Used to communicate with the sync server.
    std::unique_ptr<HttpPostProviderFactory> post_factory;

    std::unique_ptr<SyncEncryptionHandler::Observer> encryption_observer_proxy;

    // Must outlive SyncManager.
    raw_ptr<ExtensionsActivity> extensions_activity = nullptr;

    std::unique_ptr<EngineComponentsFactory> engine_components_factory;

    // Must outlive SyncManager.
    raw_ptr<SyncEncryptionHandler> encryption_handler = nullptr;

    // Carries shutdown requests across sequences and will be used to cut short
    // any network I/O and tell the syncer to exit early.
    //
    // Must outlive SyncManager.
    raw_ptr<CancelationSignal> cancelation_signal = nullptr;

    // Define the polling interval. Must not be zero.
    base::TimeDelta poll_interval;

    // Initial authoritative values (usually read from prefs).
    std::string cache_guid;
    std::string birthday;
    std::string bag_of_chips;
  };

  // The state of sync the feature. If the user turned on sync explicitly, it
  // will be set to ON. Will be set to INITIALIZING until we know the actual
  // state.
  enum class SyncFeatureState { INITIALIZING, ON, OFF };

  SyncManager();
  virtual ~SyncManager();

  // Initialize the sync manager using arguments from |args|.
  //
  // Note, args is passed by non-const pointer because it contains objects like
  // unique_ptr.
  virtual void Init(InitArgs* args) = 0;

  virtual DataTypeSet InitialSyncEndedTypes() = 0;

  virtual DataTypeSet GetConnectedTypes() = 0;

  // Update tokens that we're using in Sync. Email must stay the same.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // Clears the authentication tokens.
  virtual void InvalidateCredentials() = 0;

  // Put the syncer in normal mode ready to perform nudges and polls.
  virtual void StartSyncingNormally(base::Time last_poll_time) = 0;

  // Put syncer in configuration mode. Only configuration sync cycles are
  // performed. No local changes are committed to the server.
  virtual void StartConfiguration() = 0;

  // Switches the mode of operation to CONFIGURATION_MODE and performs
  // any configuration tasks needed as determined by the params. Once complete,
  // syncer will remain in CONFIGURATION_MODE until StartSyncingNormally is
  // called.
  // |ready_task| is invoked when the configuration completes.
  virtual void ConfigureSyncer(ConfigureReason reason,
                               DataTypeSet to_download,
                               SyncFeatureState sync_feature_state,
                               base::OnceClosure ready_task) = 0;

  // Inform the syncer of a change in the invalidator's state.
  virtual void SetInvalidatorEnabled(bool invalidator_enabled) = 0;

  // Inform the syncer that its cached information about a type is obsolete.
  virtual void OnIncomingInvalidation(
      DataType type,
      std::unique_ptr<SyncInvalidation> invalidation) = 0;

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove the given observer.  Make sure to call this if the
  // Observer is being destroyed so the SyncManager doesn't
  // potentially dereference garbage.
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void ShutdownOnSyncThread() = 0;

  // Returns non-owning pointer to DataTypeConnector. In contrast with
  // DataTypeConnectorProxy all calls are executed synchronously, thus the
  // pointer should be used on sync sequence.
  virtual DataTypeConnector* GetDataTypeConnector() = 0;

  // Returns an instance of the main interface for registering sync types with
  // sync engine.
  virtual std::unique_ptr<DataTypeConnector> GetDataTypeConnectorProxy() = 0;

  // Returns the cache_guid of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string cache_guid() = 0;

  // Returns the birthday of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string birthday() = 0;

  // Returns the bag of chips of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual std::string bag_of_chips() = 0;

  // Returns whether there are remaining unsynced items.
  virtual bool HasUnsyncedItemsForTest() = 0;

  // Returns the SyncManager's encryption handler.
  virtual SyncEncryptionHandler* GetEncryptionHandler() = 0;

  // Ask the SyncManager to fetch updates for the given types.
  virtual void RefreshTypes(DataTypeSet types) = 0;

  // Returns any buffered protocol events.  Does not clear the buffer.
  virtual std::vector<std::unique_ptr<ProtocolEvent>>
  GetBufferedProtocolEvents() = 0;

  // Updates Sync's tracking of whether the cookie jar has a mismatch with the
  // chrome account. See ClientConfigParams proto message for more info.
  // Note: this does not trigger a sync cycle. It just updates the sync context.
  virtual void OnCookieJarChanged(bool account_mismatch) = 0;

  // Updates the invalidation information from known active devices.
  virtual void UpdateActiveDevicesInvalidationInfo(
      ActiveDevicesInvalidationInfo active_devices_invalidation_info) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_MANAGER_H_
