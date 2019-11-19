// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace chromeos {

namespace sync_wifi {

// Receives updates to network configurations from the Chrome sync back end and
// from the system network stack and keeps both lists in sync.
class WifiConfigurationBridge : public syncer::ModelTypeSyncBridge {
 public:
  WifiConfigurationBridge(
      SyncedNetworkUpdater* synced_network_updater,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback);
  ~WifiConfigurationBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // Comes from |entries_| the in-memory map.
  std::vector<NetworkIdentifier> GetAllIdsForTesting();

 private:
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  // Callbacks for ModelTypeStore.
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> records);
  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<syncer::ModelError>& error);

  // An in-memory list of the proto's that mirrors what is on the sync server.
  // This gets updated when changes are received from the server and after local
  // changes have been committed.  On initialization of this class, it is
  // populated with the contents of |store_|.
  base::flat_map<std::string, sync_pb::WifiConfigurationSpecificsData> entries_;

  // The on disk store of WifiConfigurationSpecifics protos that mirrors what
  // is on the sync server.  This gets updated when changes are received from
  // the server and after local changes have been committed to the server.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  SyncedNetworkUpdater* synced_network_updater_;

  base::WeakPtrFactory<WifiConfigurationBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationBridge);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
