// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
}

namespace themes {

// Helpers to get specific proto from EntitySpecifics.
template <typename Specifics>
const Specifics& GetSpecifics(const sync_pb::EntitySpecifics& specifics);

template <>
inline const sync_pb::ThemeSpecifics& GetSpecifics<sync_pb::ThemeSpecifics>(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.theme();
}

template <>
inline const sync_pb::ThemeIosSpecifics&
GetSpecifics<sync_pb::ThemeIosSpecifics>(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.theme_ios();
}

template <>
inline const sync_pb::ThemeAndroidSpecifics&
GetSpecifics<sync_pb::ThemeAndroidSpecifics>(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.theme_android();
}

// Generic sync bridge for cross-device themes.
// It is read-only and delegates storage and updates to CrossDeviceThemeTracker.
template <typename RemoteSpecifics, typename LocalSpecifics>
class CrossDeviceThemeSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  using TranslateCallback =
      base::RepeatingCallback<DeviceThemeInfo<LocalSpecifics>(
          const RemoteSpecifics&)>;
  using UpdateCallback =
      base::RepeatingCallback<void(const std::string&,
                                   DeviceThemeInfo<LocalSpecifics>)>;
  using RemoveCallback = base::RepeatingCallback<void(const std::string&)>;
  using SyncStartedCallback = base::RepeatingClosure;
  using DisableCallback = base::RepeatingClosure;

  CrossDeviceThemeSyncBridge(
      syncer::DataType type,
      TranslateCallback translate_cb,
      UpdateCallback update_cb,
      RemoveCallback remove_cb,
      SyncStartedCallback sync_started_cb,
      DisableCallback disable_cb,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory)
      : syncer::DataTypeSyncBridge(std::move(change_processor)),
        type_(type),
        translate_cb_(std::move(translate_cb)),
        update_cb_(std::move(update_cb)),
        remove_cb_(std::move(remove_cb)),
        sync_started_cb_(std::move(sync_started_cb)),
        disable_cb_(std::move(disable_cb)) {
    std::move(store_factory)
        .Run(type_, base::BindOnce(&CrossDeviceThemeSyncBridge::OnStoreCreated,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  CrossDeviceThemeSyncBridge(const CrossDeviceThemeSyncBridge&) = delete;
  CrossDeviceThemeSyncBridge& operator=(const CrossDeviceThemeSyncBridge&) =
      delete;

  ~CrossDeviceThemeSyncBridge() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  bool IsStoreInitializedForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return store_ != nullptr;
  }

  // DataTypeSyncBridge implementation:
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    syncer::DataTypeSyncBridge::OnSyncStarting(request);
    if (sync_started_cb_) {
      sync_started_cb_.Run();
    }
  }

  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  }

  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                       std::move(entity_changes));
  }

  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch(std::move(metadata_change_list));

    for (const auto& change : entity_changes) {
      if (change->type() == syncer::EntityChange::ACTION_DELETE) {
        batch->DeleteData(change->storage_key());
        remove_cb_.Run(change->storage_key());
      } else {
        const sync_pb::EntitySpecifics& specifics = change->data().specifics;
        const RemoteSpecifics& platform_specifics =
            GetSpecifics<RemoteSpecifics>(specifics);
        DeviceThemeInfo<LocalSpecifics> theme_info =
            translate_cb_.Run(platform_specifics);

        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        update_cb_.Run(change->storage_key(), std::move(theme_info));
      }
    }

    Commit(std::move(batch));
    return std::nullopt;
  }

  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override {
    NOTREACHED();
  }

  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override {
    return nullptr;
  }

  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override {
    return entity_specifics;
  }

  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override {
    return true;
  }

  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override {
    return GetStorageKey(entity_data);
  }

  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override {
    return entity_data.client_tag_hash.value();
  }

  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!store_) {
      return;
    }
    store_->DeleteAllDataAndMetadata(
        std::move(delete_metadata_change_list),
        base::BindOnce(&CrossDeviceThemeSyncBridge::OnDatabaseDeleted,
                       weak_ptr_factory_.GetWeakPtr()));
    if (disable_cb_) {
      disable_cb_.Run();
    }
  }

 private:
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store) {
    if (error) {
      change_processor()->ReportError(*error);
      return;
    }
    store_ = std::move(store);
    store_->ReadAllDataAndMetadata(
        base::BindOnce(&CrossDeviceThemeSyncBridge::OnReadAllDataAndMetadata,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
    if (error) {
      change_processor()->ReportError(*error);
      return;
    }

    for (const auto& record : *data_records) {
      sync_pb::EntitySpecifics specifics;
      if (specifics.ParseFromString(record.value)) {
        const RemoteSpecifics& platform_specifics =
            GetSpecifics<RemoteSpecifics>(specifics);
        DeviceThemeInfo<LocalSpecifics> theme_info =
            translate_cb_.Run(platform_specifics);
        update_cb_.Run(record.id, std::move(theme_info));
      }
    }

    change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }

  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(&CrossDeviceThemeSyncBridge::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnCommitResult(const std::optional<syncer::ModelError>& error) {
    if (error) {
      change_processor()->ReportError(*error);
    }
  }

  void OnDatabaseDeleted(const std::optional<syncer::ModelError>& error) {
    if (error) {
      change_processor()->ReportError(*error);
    }
  }

  const syncer::DataType type_;
  const TranslateCallback translate_cb_;
  const UpdateCallback update_cb_;
  const RemoveCallback remove_cb_;
  const SyncStartedCallback sync_started_cb_;
  const DisableCallback disable_cb_;

  std::unique_ptr<syncer::DataTypeStore> store_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CrossDeviceThemeSyncBridge> weak_ptr_factory_{this};
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_
