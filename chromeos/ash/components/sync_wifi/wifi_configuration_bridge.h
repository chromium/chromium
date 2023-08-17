// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_metadata_observer.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace ash {

class NetworkConfigurationHandler;
class NetworkMetadataStore;

namespace sync_wifi {

const char kIsFirstRun[] = "sync_wifi.is_first_run";
const char kHasFixedAutoconnect[] = "sync_wifi.has_fixed_autoconnect";

class LocalNetworkCollector;
class SyncedNetworkMetricsLogger;
class SyncedNetworkUpdater;
class TimerFactory;

// Receives updates to network configurations from the Chrome sync back end and
// from the system network stack and keeps both lists in sync.
class WifiConfigurationBridge : public syncer::ModelTypeSyncBridge,
                                public NetworkConfigurationObserver,
                                public NetworkMetadataObserver {
 public:
  WifiConfigurationBridge(
      SyncedNetworkUpdater* synced_network_updater,
      LocalNetworkCollector* local_network_collector,
      NetworkConfigurationHandler* network_configuration_handler,
      SyncedNetworkMetricsLogger* metrics_recorder,
      TimerFactory* timer_factory,
      PrefService* pref_service,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback);

  WifiConfigurationBridge(const WifiConfigurationBridge&) = delete;
  WifiConfigurationBridge& operator=(const WifiConfigurationBridge&) = delete;

  ~WifiConfigurationBridge() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // NetworkMetadataObserver:
  void OnFirstConnectionToNetwork(const std::string& guid) override;
  void OnNetworkCreated(const std::string& guid) override;
  void OnNetworkUpdate(const std::string& guid,
                       const base::Value::Dict* set_properties) override;

  // NetworkConfigurationObserver::
  void OnBeforeConfigurationRemoved(const std::string& service_path,
                                    const std::string& guid) override;
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;
  void OnShuttingDown() override;

  // Comes from |entries_| the in-memory map.
  std::vector<NetworkIdentifier> GetAllIdsForTesting();

  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store);

 private:
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  // Callbacks for ModelTypeStore.
  void OnStoreCreated(const absl::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> records);
  void OnReadAllMetadata(const absl::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const absl::optional<syncer::ModelError>& error);

  void OnGetAllSyncableNetworksResult(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList change_list,
      std::vector<sync_pb::WifiConfigurationSpecifics> local_network_list);

  void SaveNetworkToSync(
      absl::optional<sync_pb::WifiConfigurationSpecifics> proto);
  void RemoveNetworkFromSync(const std::string& storage_key);

  // Starts an async request to serialize a network to a proto and save to sync.
  void OnNetworkConfiguredDelayComplete(const std::string& network_guid);

  bool IsLastUpdateFromSync(const std::string& network_guid);

  void FixAutoconnect();
  void OnFixAutoconnectComplete();

  // An in-memory list of the proto's that mirrors what is on the sync server.
  // This gets updated when changes are received from the server and after local
  // changes have been committed.  On initialization of this class, it is
  // populated with the contents of |store_|.
  base::flat_map<std::string, sync_pb::WifiConfigurationSpecifics> entries_;

  // Map of network |guid| to |storage_key|.  After a network is deleted, we
  // no longer have access to its metadata so this stores the necessary
  // information to delete it from sync.
  base::flat_map<std::string, std::string> pending_deletes_;

  // Holds on to timers that are started immediately after a network is
  // configured so we can wait until the first connection attempt is complete.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      network_guid_to_timer_map_;

  // Map of storage_key to proto which tracks networks that should be synced
  // once the service is ready.  This is keyed on network_id to ensure that the
  // most recent change is kept if there are multiple changes to the same
  // network.
  base::flat_map<std::string,
                 absl::optional<sync_pb::WifiConfigurationSpecifics>>
      networks_to_sync_when_ready_;

  // The on disk store of WifiConfigurationSpecifics protos that mirrors what
  // is on the sync server.  This gets updated when changes are received from
  // the server and after local changes have been committed to the server.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  raw_ptr<SyncedNetworkUpdater, DanglingUntriaged | ExperimentalAsh>
      synced_network_updater_;
  raw_ptr<LocalNetworkCollector, DanglingUntriaged | ExperimentalAsh>
      local_network_collector_;
  raw_ptr<NetworkConfigurationHandler, ExperimentalAsh>
      network_configuration_handler_;
  raw_ptr<SyncedNetworkMetricsLogger, DanglingUntriaged | ExperimentalAsh>
      metrics_recorder_;
  raw_ptr<TimerFactory, DanglingUntriaged | ExperimentalAsh> timer_factory_;
  raw_ptr<PrefService, DanglingUntriaged | ExperimentalAsh> pref_service_;
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;

  base::WeakPtrFactory<WifiConfigurationBridge> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
