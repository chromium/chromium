// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"

#include <optional>

#include "base/notimplemented.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"

namespace tab_groups {

SharedTabGroupAccountDataSyncBridge::SharedTabGroupAccountDataSyncBridge(
    std::unique_ptr<SyncDataTypeConfiguration> configuration)
    : syncer::DataTypeSyncBridge(std::move(configuration->change_processor)) {
  std::move(configuration->data_type_store_factory)
      .Run(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
           base::BindOnce(&SharedTabGroupAccountDataSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SharedTabGroupAccountDataSyncBridge::~SharedTabGroupAccountDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
SharedTabGroupAccountDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

std::string SharedTabGroupAccountDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return "";
}

std::string SharedTabGroupAccountDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return "";
}

void SharedTabGroupAccountDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool SharedTabGroupAccountDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

sync_pb::EntitySpecifics
SharedTabGroupAccountDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return sync_pb::EntitySpecifics();
}

void SharedTabGroupAccountDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);

  // TODO(crbug.com/397767033): Read all data and metadata
}

}  // namespace tab_groups
