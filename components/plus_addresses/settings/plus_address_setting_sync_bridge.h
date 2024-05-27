// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_BRIDGE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_BRIDGE_H_

#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace plus_addresses {

// Bridge for PLUS_ADDRESS_SETTING. Lives on the UI thread and is owned by
// `PlusAddressSettingService`.
class PlusAddressSettingSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  explicit PlusAddressSettingSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory store_factory);
  ~PlusAddressSettingSyncBridge() override = default;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SYNC_BRIDGE_H_
