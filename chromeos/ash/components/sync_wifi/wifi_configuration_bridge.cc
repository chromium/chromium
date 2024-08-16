// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/wifi_configuration_bridge.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/sync_wifi/local_network_collector.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/network_type_conversions.h"
#include "chromeos/ash/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/ash/components/sync_wifi/synced_network_updater.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {

std::unique_ptr<syncer::EntityData> GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecifics& proto) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_wifi_configuration()->CopyFrom(proto);
  entity_data->name = NetworkIdentifier::FromProto(proto).SerializeToString();
  return entity_data;
}

// Delay before attempting to save a newly configured network to sync.  This
// is to give time for an initial connection attempt to fail in case of a bad
// password, which will prevent syncing.
constexpr base::TimeDelta kSyncAfterCreatedTimeout = base::Seconds(20);

}  // namespace

WifiConfigurationBridge::WifiConfigurationBridge(
    SyncedNetworkUpdater* synced_network_updater,
    LocalNetworkCollector* local_network_collector,
    NetworkConfigurationHandler* network_configuration_handler,
    SyncedNetworkMetricsLogger* metrics_recorder,
    ash::timer_factory::TimerFactory* timer_factory,
    PrefService* pref_service,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory create_store_callback)
    : DataTypeSyncBridge(std::move(change_processor)),
      synced_network_updater_(synced_network_updater),
      local_network_collector_(local_network_collector),
      network_configuration_handler_(network_configuration_handler),
      metrics_recorder_(metrics_recorder),
      timer_factory_(timer_factory),
      pref_service_(pref_service),
      network_metadata_store_(nullptr) {
  std::move(create_store_callback)
      .Run(syncer::WIFI_CONFIGURATIONS,
           base::BindOnce(&WifiConfigurationBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
  if (network_configuration_handler_) {
    network_configuration_handler_->AddObserver(this);
  }
}

WifiConfigurationBridge::~WifiConfigurationBridge() {
  OnShuttingDown();
}

// static
void WifiConfigurationBridge::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kIsFirstRun, true);
  registry->RegisterBooleanPref(kHasFixedAutoconnect, false);
}

void WifiConfigurationBridge::OnShuttingDown() {
  if (network_metadata_store_) {
    network_metadata_store_->RemoveObserver(this);
    network_metadata_store_ = nullptr;
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->RemoveObserver(this);
    network_configuration_handler_ = nullptr;
  }
}

std::unique_ptr<syncer::MetadataChangeList>
WifiConfigurationBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> WifiConfigurationBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList change_list) {
  DCHECK(entries_.empty());
  DCHECK(local_network_collector_);

  local_network_collector_->GetAllSyncableNetworks(
      base::BindOnce(&WifiConfigurationBridge::OnGetAllSyncableNetworksResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(metadata_change_list), std::move(change_list)));

  return std::nullopt;
}

void WifiConfigurationBridge::OnGetAllSyncableNetworksResult(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList change_list,
    std::vector<sync_pb::WifiConfigurationSpecifics> local_network_list) {
  // To merge local and synced networks we add all local networks that don't
  // exist in sync to the server and all synced networks that don't exist
  // locally to Shill.  For networks which exist on both lists, we compare the
  // last connected timestamp and take the newer configuration.

  NET_LOG(EVENT) << "Merging " << local_network_list.size() << " local and "
                 << change_list.size() << " synced networks.";
  base::flat_map<NetworkIdentifier, sync_pb::WifiConfigurationSpecifics>
      sync_networks;
  base::flat_map<NetworkIdentifier, sync_pb::WifiConfigurationSpecifics>
      local_networks;

  // Iterate through incoming changes from sync and populate the sync_networks
  // map.
  for (std::unique_ptr<syncer::EntityChange>& change : change_list) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      // Don't delete any local networks during the initial merge when sync is
      // first enabled.
      continue;
    }

    const sync_pb::WifiConfigurationSpecifics& proto =
        change->data().specifics.wifi_configuration();
    NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
    if (sync_networks.contains(id) &&
        sync_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }
    sync_networks[id] = proto;
  }

  // Iterate through local networks and add to sync where appropriate.
  for (sync_pb::WifiConfigurationSpecifics& proto : local_network_list) {
    NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
    if (sync_networks.contains(id) &&
        sync_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }

    local_networks[id] = proto;
    std::unique_ptr<syncer::EntityData> entity_data =
        GenerateWifiEntityData(proto);
    std::string storage_key = GetStorageKey(*entity_data);

    // Upload the local network configuration to sync.  This could be a new
    // configuration or an update to an existing one.
    change_processor()->Put(storage_key, std::move(entity_data),
                            metadata_change_list.get());
    entries_[storage_key] = proto;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  // Iterate through synced networks and update local stack where appropriate.
  for (const auto& [id, proto] : sync_networks) {
    if (local_networks.contains(id) &&
        local_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }

    // Update the local network stack to have the synced network configuration.
    synced_network_updater_->AddOrUpdateNetwork(proto);

    // Save the proto to the sync data store to keep track of all synced
    // networks on device.  This gets loaded into |entries_| next time the
    // bridge is initialized.
    batch->WriteData(id.SerializeToString(), proto.SerializeAsString());
    entries_[id.SerializeToString()] = proto;
  }

  // Mark the changes as processed.
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
  metrics_recorder_->RecordTotalCount(entries_.size());
  // If zero networks are synced log the reason.
  if (entries_.size() == 0) {
    local_network_collector_->RecordZeroNetworksEligibleForSync();
  }
}

std::optional<syncer::ModelError>
WifiConfigurationBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  NET_LOG(EVENT) << "Applying  " << entity_changes.size()
                 << " pending changes.";

  // TODO(jonmann) Don't override synced network configurations that are newer
  // than the local configurations.
  for (std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      auto it = entries_.find(change->storage_key());
      if (it != entries_.end()) {
        entries_.erase(it);
        batch->DeleteData(change->storage_key());
        if (!base::FeatureList::IsEnabled(features::kWifiSyncApplyDeletes)) {
          // Don't apply deletes to the local device.
          NET_LOG(EVENT) << "Ignoring delete request from sync server.";
          continue;
        }
        synced_network_updater_->RemoveNetwork(
            NetworkIdentifier::DeserializeFromString(change->storage_key()));
      } else {
        NET_LOG(EVENT) << "Received delete request for network which is not "
                          "tracked by sync.";
      }
      continue;
    }

    auto& specifics = change->data().specifics.wifi_configuration();
    synced_network_updater_->AddOrUpdateNetwork(specifics);

    batch->WriteData(change->storage_key(), specifics.SerializeAsString());
    entries_[change->storage_key()] = std::move(specifics);
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
  metrics_recorder_->RecordTotalCount(entries_.size());
  // If zero networks are synced log the reason.
  if (entries_.size() == 0) {
    local_network_collector_->RecordZeroNetworksEligibleForSync();
  }

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> WifiConfigurationBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& id : storage_keys) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
      continue;
    }
    batch->Put(id, GenerateWifiEntityData(it->second));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
WifiConfigurationBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : entries_) {
    batch->Put(storage_key, GenerateWifiEntityData(specifics));
  }
  return batch;
}

std::string WifiConfigurationBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string WifiConfigurationBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return NetworkIdentifier::FromProto(
             entity_data.specifics.wifi_configuration())
      .SerializeToString();
}

void WifiConfigurationBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Since bridge and DataTypeStore state represents the synced networks state,
  // while actual data is stored by Shill, it's appropriate to treat all data
  // stored by bridge as metadata and clear it out when processor requests to
  // clear metadata. MergeFullSyncData() will be called once sync is starting
  // again.
  entries_.clear();
  pending_deletes_.clear();
  network_guid_to_timer_map_.clear();
  networks_to_sync_when_ready_.clear();
  if (store_) {
    store_->DeleteAllDataAndMetadata(base::DoNothing());
  }
  // Callbacks are no longer valid.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WifiConfigurationBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&WifiConfigurationBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> records) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (syncer::DataTypeStore::Record& record : *records) {
    sync_pb::WifiConfigurationSpecifics data;
    if (record.id.empty() || !data.ParseFromString(record.value)) {
      NET_LOG(EVENT) << "Unable to parse proto for entry with key: "
                     << record.id;
      continue;
    }
    entries_[record.id] = std::move(data);
  }
  store_->ReadAllMetadata(
      base::BindOnce(&WifiConfigurationBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));

  // Temporary fix for networks which accidentally had autoconnect disabled.
  if (!pref_service_->GetBoolean(kHasFixedAutoconnect)) {
    local_network_collector_->ExecuteAfterNetworksLoaded(
        base::BindOnce(&WifiConfigurationBridge::FixAutoconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  int entries_size = entries_.size();
  // Do not log the total network count during OOBE. It returns 0 even if there
  // are networks synced since MergeFullSyncData has not executed yet.
  if (pref_service_->GetBoolean(kIsFirstRun)) {
    pref_service_->SetBoolean(kIsFirstRun, false);
    // This is only meant to filter out 0's that are logged during OOBE. If the
    // entries_size is greater than zero it should be logged.
    if (entries_size == 0) {
      return;
    }
  }
  metrics_recorder_->RecordTotalCount(entries_size);
  // If zero networks are synced log the reason.
  if (entries_size == 0) {
    local_network_collector_->RecordZeroNetworksEligibleForSync();
  }
}

void WifiConfigurationBridge::FixAutoconnect() {
  // Temporary fix for networks which accidentally had autoconnect disabled.
  if (!pref_service_->GetBoolean(kHasFixedAutoconnect)) {
    std::vector<sync_pb::WifiConfigurationSpecifics> protos;
    for (const auto& [storage_key, specifics] : entries_) {
      protos.push_back(specifics);
    }
    local_network_collector_->FixAutoconnect(
        protos,
        base::BindOnce(&WifiConfigurationBridge::OnFixAutoconnectComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void WifiConfigurationBridge::OnFixAutoconnectComplete() {
  pref_service_->SetBoolean(kHasFixedAutoconnect, true);
}

void WifiConfigurationBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "WifiConfigurationBridge::OnReadAllMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Make a copy in case the map is modified while iterating over the pending
  // updates.  This could happen if sync is disabled while iterating.
  base::flat_map<std::string,
                 std::optional<sync_pb::WifiConfigurationSpecifics>>
      updates = networks_to_sync_when_ready_;
  for (auto const& [storage_key, specifics] : updates) {
    if (specifics) {
      SaveNetworkToSync(specifics);
      continue;
    }
    RemoveNetworkFromSync(storage_key);
  }
  networks_to_sync_when_ready_.clear();
}

void WifiConfigurationBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error)
    change_processor()->ReportError(*error);
}

void WifiConfigurationBridge::Commit(
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&WifiConfigurationBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

std::vector<NetworkIdentifier> WifiConfigurationBridge::GetAllIdsForTesting() {
  std::vector<NetworkIdentifier> ids;
  for (const auto& [storage_key, specifics] : entries_)
    ids.push_back(NetworkIdentifier::FromProto(specifics));

  return ids;
}

void WifiConfigurationBridge::OnFirstConnectionToNetwork(
    const std::string& guid) {
  if (network_guid_to_timer_map_.contains(guid)) {
    network_guid_to_timer_map_.erase(guid);
  }

  if (network_metadata_store_->GetIsConfiguredBySync(guid)) {
    // Don't have to upload a configuration that came from sync.
    NET_LOG(EVENT) << "Not uploading network on first connect: "
                   << NetworkGuidId(guid) << " was added by sync.";
    return;
  }

  NET_LOG(EVENT) << "Syncing network after first connect: "
                 << NetworkGuidId(guid);
  local_network_collector_->GetSyncableNetwork(
      guid, base::BindOnce(&WifiConfigurationBridge::SaveNetworkToSync,
                           weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnNetworkUpdate(
    const std::string& guid,
    const base::Value::Dict* set_properties) {
  if (!set_properties)
    return;

  if (synced_network_updater_->IsUpdateInProgress(guid) ||
      network_metadata_store_->GetIsConfiguredBySync(guid)) {
    // Don't have to upload a configuration that came from sync.
    NET_LOG(EVENT) << "Not uploading change to " << NetworkGuidId(guid)
                   << ", modified network was configured "
                      "by sync.";
    return;
  }

  if (!set_properties->contains(shill::kAutoConnectProperty) &&
      !set_properties->contains(shill::kPriorityProperty) &&
      !set_properties->contains(shill::kProxyConfigProperty) &&
      !set_properties->contains(shill::kMeteredProperty) &&
      !set_properties->FindByDottedPath(
          base::StringPrintf("%s.%s", shill::kStaticIPConfigProperty,
                             shill::kNameServersProperty))) {
    NET_LOG(EVENT) << "Not uploading change to " << NetworkGuidId(guid)
                   << ", modified network field(s) are not synced.";
    return;
  }

  NET_LOG(EVENT) << "Updating sync with changes to " << NetworkGuidId(guid);
  local_network_collector_->GetSyncableNetwork(
      guid, base::BindOnce(&WifiConfigurationBridge::SaveNetworkToSync,
                           weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::SaveNetworkToSync(
    std::optional<sync_pb::WifiConfigurationSpecifics> proto) {
  if (!proto) {
    return;
  }

  // If the sync backend hasn't finished initializing the bridge, then store
  // this update to be processed later.
  if (!store_ || !change_processor()->IsTrackingMetadata()) {
    networks_to_sync_when_ready_.insert_or_assign(
        NetworkIdentifier::FromProto(*proto).SerializeToString(), proto);
    return;
  }

  std::unique_ptr<syncer::EntityData> entity_data =
      GenerateWifiEntityData(*proto);
  auto id = NetworkIdentifier::FromProto(*proto);
  std::string storage_key = GetStorageKey(*entity_data);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(storage_key, proto->SerializeAsString());
  change_processor()->Put(storage_key, std::move(entity_data),
                          batch->GetMetadataChangeList());
  entries_[storage_key] = *proto;
  Commit(std::move(batch));
  NET_LOG(EVENT) << "Saved network "
                 << NetworkId(NetworkStateFromNetworkIdentifier(id))
                 << " to sync.";
  metrics_recorder_->RecordTotalCount(entries_.size());
  // If zero networks are synced log the reason.
  if (entries_.size() == 0) {
    local_network_collector_->RecordZeroNetworksEligibleForSync();
  }
}

void WifiConfigurationBridge::OnNetworkCreated(const std::string& guid) {
  network_guid_to_timer_map_[guid] = timer_factory_->CreateOneShotTimer();
  network_guid_to_timer_map_[guid]->Start(
      FROM_HERE, kSyncAfterCreatedTimeout,
      base::BindOnce(&WifiConfigurationBridge::OnNetworkConfiguredDelayComplete,
                     weak_ptr_factory_.GetWeakPtr(), guid));
}

void WifiConfigurationBridge::OnNetworkConfiguredDelayComplete(
    const std::string& network_guid) {
  if (network_guid_to_timer_map_.contains(network_guid)) {
    network_guid_to_timer_map_.erase(network_guid);
  }

  // This check to prevent uploading networks that were added by sync happens
  // after the delay because the metadata isn't available in OnNetworkCreated.
  if (network_metadata_store_->GetIsConfiguredBySync(network_guid)) {
    NET_LOG(EVENT) << "Not uploading newly configured network "
                   << NetworkGuidId(network_guid) << ", it was added by sync.";
    return;
  }

  NET_LOG(EVENT) << "Attempting to sync new network after delay.";
  local_network_collector_->GetSyncableNetwork(
      network_guid, base::BindOnce(&WifiConfigurationBridge::SaveNetworkToSync,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnBeforeConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {
  if (!base::FeatureList::IsEnabled(features::kWifiSyncAllowDeletes)) {
    return;
  }

  std::optional<NetworkIdentifier> id =
      local_network_collector_->GetNetworkIdentifierFromGuid(guid);
  if (!id) {
    return;
  }

  NET_LOG(EVENT) << "Storing metadata for " << NetworkPathId(service_path)
                 << " in preparation for removal.";
  pending_deletes_[guid] = id->SerializeToString();
}

void WifiConfigurationBridge::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& network_guid) {
  if (!base::FeatureList::IsEnabled(features::kWifiSyncAllowDeletes)) {
    return;
  }

  if (!pending_deletes_.contains(network_guid)) {
    NET_LOG(EVENT) << "Configuration " << network_guid
                   << " removed with no matching saved metadata.";
    return;
  }

  const std::string& storage_key = pending_deletes_[network_guid];
  if (!store_ || !change_processor()->IsTrackingMetadata()) {
    networks_to_sync_when_ready_.insert_or_assign(storage_key, std::nullopt);
    return;
  }

  RemoveNetworkFromSync(storage_key);
}

void WifiConfigurationBridge::RemoveNetworkFromSync(
    const std::string& storage_key) {
  if (!base::FeatureList::IsEnabled(features::kWifiSyncAllowDeletes)) {
    return;
  }
  if (!entries_.contains(storage_key)) {
    return;  // Network is not synced.
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->DeleteData(storage_key);
  change_processor()->Delete(storage_key, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());
  entries_.erase(storage_key);
  Commit(std::move(batch));
  NET_LOG(EVENT) << "Removed network from sync.";
}

void WifiConfigurationBridge::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {
  if (network_metadata_store_) {
    network_metadata_store->RemoveObserver(this);
  }
  network_metadata_store_ = network_metadata_store;
  network_metadata_store->AddObserver(this);
}

}  // namespace ash::sync_wifi
