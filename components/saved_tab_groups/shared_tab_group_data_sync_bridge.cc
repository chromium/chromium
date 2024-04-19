// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"

namespace tab_groups {

SharedTabGroupDataSyncBridge::SharedTabGroupDataSyncBridge(
    SavedTabGroupModel* model,
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)), model_(model) {
  CHECK(model_);
  std::move(create_store_callback)
      .Run(syncer::SHARED_TAB_GROUP_DATA,
           base::BindOnce(&SharedTabGroupDataSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SharedTabGroupDataSyncBridge::~SharedTabGroupDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
SharedTabGroupDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::nullopt;
}

void SharedTabGroupDataSyncBridge::GetData(StorageKeyList storage_keys,
                                           DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

void SharedTabGroupDataSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

std::string SharedTabGroupDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string SharedTabGroupDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.shared_tab_group_data().guid();
}

bool SharedTabGroupDataSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool SharedTabGroupDataSyncBridge::SupportsGetStorageKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool SharedTabGroupDataSyncBridge::SupportsIncrementalUpdates() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void SharedTabGroupDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

sync_pb::EntitySpecifics
SharedTabGroupDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return ModelTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
      entity_specifics);
}

bool SharedTabGroupDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return true;
}

void SharedTabGroupDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseLoad,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::OnDatabaseLoad(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_->ReadAllMetadata(
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entries)));
}

void SharedTabGroupDataSyncBridge::OnReadAllMetadata(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to read metadata."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // TODO(crbug.com/319521964): Process result.
}

}  // namespace tab_groups
