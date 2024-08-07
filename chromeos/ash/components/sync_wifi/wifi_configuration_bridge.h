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
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace ash::timer_factory {
class TimerFactory;
}  // namespace ash::timer_factory

namespace ash {

class NetworkConfigurationHandler;
class NetworkMetadataStore;

namespace sync_wifi {

const char kIsFirstRun[] = "sync_wifi.is_first_run";
const char kHasFixedAutoconnect[] = "sync_wifi.has_fixed_autoconnect";

class LocalNetworkCollector;
class SyncedNetworkMetricsLogger;
class SyncedNetworkUpdater;

// Receives updates to network configurations from the Chrome sync back end and
// from the system network stack and keeps both lists in sync.
class WifiConfigurationBridge : public syncer::DataTypeSyncBridge,
                                public NetworkConfigurationObserver,
                                public NetworkMetadataObserver {
 public:
  WifiConfigurationBridge(
      SyncedNetworkUpdater* synced_network_updater,
      LocalNetworkCollector* local_network_collector,
      NetworkConfigurationHandler* network_configuration_handler,
      SyncedNetworkMetricsLogger* metrics_recorder,
      ash::timer_factory::TimerFactory* timer_factory,
      PrefService* pref_service,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory create_store_callback);

  WifiConfigurationBridge(const WifiConfigurationBridge&) = delete;
  WifiConfigurationBridge& operator=(const WifiConfigurationBridge&) = delete;

  ~WifiConfigurationBridge() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
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
  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch);

  // Callbacks for DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> records);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const std::optional<syncer::ModelError>& error);

  void OnGetAllSyncableNetworksResult(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList change_list,
      std::vector<sync_pb::WifiConfigurationSpecifics> local_network_list);

  void SaveNetworkToSync(
      std::optional<sync_pb::WifiConfigurationSpecifics> proto);
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
                 std::optional<sync_pb::WifiConfigurationSpecifics>>
      networks_to_sync_when_ready_;

  // The on disk store of WifiConfigurationSpecifics protos that mirrors what
  // is on the sync server.  This gets updated when changes are received from
  // the server and after local changes have been committed to the server.
  std::unique_ptr<syncer::DataTypeStore> store_;

  raw_ptr<SyncedNetworkUpdater, DanglingUntriaged> synced_network_updater_;
  raw_ptr<LocalNetworkCollector, DanglingUntriaged> local_network_collector_;
  raw_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  raw_ptr<SyncedNetworkMetricsLogger, DanglingUntriaged> metrics_recorder_;
  raw_ptr<ash::timer_factory::TimerFactory, DanglingUntriaged> timer_factory_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;

  base::WeakPtrFactory<WifiConfigurationBridge> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
