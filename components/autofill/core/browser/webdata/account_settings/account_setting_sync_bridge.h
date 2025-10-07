// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_BRIDGE_H_

#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace autofill {

// Bridge for ACCOUNT_SETTING. Lives on the UI thread and is owned by
// `AccountSettingService`.
class AccountSettingSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit AccountSettingSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);
  ~AccountSettingSyncBridge() override;

  // Returns the value for the setting of the given `name` if the bridge
  // is aware of any such setting, and the setting has the expected type.
  // Otherwise, nullopt is returned.
  // Virtual for testing.
  virtual std::optional<bool> GetBoolSetting(std::string_view name) const;
  virtual std::optional<int> GetIntSetting(std::string_view name) const;
  virtual std::optional<std::string> GetStringSetting(
      std::string_view name) const;

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
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;

 private:
  // Callbacks for various asynchronous operations of the `store_`.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void StartSyncingWithDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ReportErrorIfSet(const std::optional<syncer::ModelError>& error);

  // Storage layer used by this sync bridge. Asynchronously created through the
  // `store_factory` injected through the constructor. Non-null if creation
  // finished without an error.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // A copy of the settings from the `store_`, used for synchronous access.
  // Keyed by `AccountSettingSpecifics::name`.
  base::flat_map<std::string, sync_pb::AccountSettingSpecifics> settings_;

  // Sequence checker ensuring that callbacks from the `store_` happen on the
  // bridge's thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountSettingSyncBridge> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SYNC_BRIDGE_H_
