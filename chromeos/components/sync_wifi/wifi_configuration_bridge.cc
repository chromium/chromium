// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace chromeos {

namespace sync_wifi {

namespace {

std::unique_ptr<syncer::EntityData> GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecificsData& data) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_wifi_configuration()
      ->mutable_client_only_encrypted_data()
      ->CopyFrom(data);
  entity_data->name = NetworkIdentifier::FromProto(data).SerializeToString();
  return entity_data;
}
}  // namespace

WifiConfigurationBridge::WifiConfigurationBridge(
    SyncedNetworkUpdater* synced_network_updater,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : ModelTypeSyncBridge(std::move(change_processor)),
      synced_network_updater_(synced_network_updater) {
  std::move(create_store_callback)
      .Run(syncer::WIFI_CONFIGURATIONS,
           base::BindOnce(&WifiConfigurationBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

WifiConfigurationBridge::~WifiConfigurationBridge() {}

std::unique_ptr<syncer::MetadataChangeList>
WifiConfigurationBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

base::Optional<syncer::ModelError> WifiConfigurationBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(entries_.empty());
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

base::Optional<syncer::ModelError> WifiConfigurationBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      auto it = entries_.find(change->storage_key());
      if (it != entries_.end()) {
        entries_.erase(it);
        batch->DeleteData(change->storage_key());
        synced_network_updater_->RemoveNetwork(
            NetworkIdentifier::DeserializeFromString(change->storage_key()));
      }
      continue;
    }

    auto& specifics = change->data()
                          .specifics.wifi_configuration()
                          .client_only_encrypted_data();
    synced_network_updater_->AddOrUpdateNetwork(specifics);

    batch->WriteData(change->storage_key(), specifics.SerializeAsString());
    entries_[change->storage_key()] = std::move(specifics);
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));

  return base::nullopt;
}

void WifiConfigurationBridge::GetData(StorageKeyList storage_keys,
                                      DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& id : storage_keys) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
      continue;
    }
    batch->Put(id, GenerateWifiEntityData(it->second));
  }
  std::move(callback).Run(std::move(batch));
}

void WifiConfigurationBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& entry : entries_) {
    batch->Put(entry.first, GenerateWifiEntityData(entry.second));
  }
  std::move(callback).Run(std::move(batch));
}

std::string WifiConfigurationBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string WifiConfigurationBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return NetworkIdentifier::FromProto(entity_data.specifics.wifi_configuration()
                                          .client_only_encrypted_data())
      .SerializeToString();
}

void WifiConfigurationBridge::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&WifiConfigurationBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnReadAllData(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> records) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (syncer::ModelTypeStore::Record& record : *records) {
    sync_pb::WifiConfigurationSpecificsData data;
    if (record.id.empty() || !data.ParseFromString(record.value)) {
      DVLOG(1) << "Unable to parse proto for entry with key: " << record.id;
      continue;
    }
    entries_[record.id] = std::move(data);
  }

  store_->ReadAllMetadata(
      base::BindOnce(&WifiConfigurationBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnReadAllMetadata(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void WifiConfigurationBridge::OnCommit(
    const base::Optional<syncer::ModelError>& error) {
  if (error)
    change_processor()->ReportError(*error);
}

void WifiConfigurationBridge::Commit(
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&WifiConfigurationBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

std::vector<NetworkIdentifier> WifiConfigurationBridge::GetAllIdsForTesting() {
  std::vector<NetworkIdentifier> ids;
  for (const auto& entry : entries_)
    ids.push_back(NetworkIdentifier::FromProto(entry.second));

  return ids;
}

}  // namespace sync_wifi

}  // namespace chromeos
