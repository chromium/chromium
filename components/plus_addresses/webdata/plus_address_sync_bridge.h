// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/model/data_type_sync_bridge.h"

class WebDatabaseBackend;

namespace plus_addresses {

class PlusAddressDataChange;
class PlusAddressTable;

class PlusAddressSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  using DataChangedBySyncCallback = base::RepeatingCallback<void(
      std::vector<PlusAddressDataChange> /*changes*/)>;
  PlusAddressSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      scoped_refptr<WebDatabaseBackend> db_backend,
      DataChangedBySyncCallback notify_data_changed_by_sync);
  ~PlusAddressSyncBridge() override;

  PlusAddressSyncBridge(const PlusAddressSyncBridge&) = delete;
  PlusAddressSyncBridge& operator=(const PlusAddressSyncBridge&) = delete;

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

 private:
  PlusAddressTable* GetPlusAddressTable();

  // `PlusAddressTable` implements `syncer::SyncMetadataStore` and stores
  // metadata for PLUS_ADDRESS. To ensures that metadata and model data is
  // committed in a single transaction, `CreateMetadataChangeList()` is
  // implemented using an `InMemoryMetadataChangeList`. This function transfers
  // the changes from the `metadata_change_list` to `GetPlusAddressTable()`. It
  // assumes that `metadata_change_list` was created using the bridge's
  // `CreateMetadataChangeList()`.
  std::optional<syncer::ModelError> TransferMetadataChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list);

  // Used to access the `WebDatabase` and its `PlusAddressTable`.
  // Depending on the entire `WebDatabaseBackend` is unnecessary, given that
  // only `WebDatabase` is used. However, since only the backend is ref-counted,
  // arguing about the lifetime is easier this way.
  const scoped_refptr<WebDatabaseBackend> db_backend_;

  // The bridges writes to the database directly (rather than through
  // `PlusAddressWebDataService`). Whenever it does, it notifies observers
  // about these changes through this callback.
  DataChangedBySyncCallback notify_data_changed_by_sync_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_BRIDGE_H_
