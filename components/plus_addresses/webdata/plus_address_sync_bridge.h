// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/sync/model/model_type_sync_bridge.h"

class WebDatabaseBackend;

namespace plus_addresses {

class PlusAddressTable;

class PlusAddressSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  PlusAddressSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      scoped_refptr<WebDatabaseBackend> db_backend);
  ~PlusAddressSyncBridge() override;

  PlusAddressSyncBridge(const PlusAddressSyncBridge&) = delete;
  PlusAddressSyncBridge& operator=(const PlusAddressSyncBridge&) = delete;

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
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

 private:
  PlusAddressTable* GetPlusAddressTable();

  // Used to access `PlusAddressTable` and commit changes.
  const scoped_refptr<WebDatabaseBackend> db_backend_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_
