// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/notimplemented.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

AccountSettingSyncBridge::AccountSettingSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : DataTypeSyncBridge(std::move(change_processor)) {}

std::unique_ptr<syncer::MetadataChangeList>
AccountSettingSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> AccountSettingSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
AccountSettingSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> AccountSettingSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // ACCOUNT_SETTING is read-only, so `GetDataForCommit()` is not needed.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
AccountSettingSyncBridge::GetAllDataForDebugging() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool AccountSettingSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  NOTIMPLEMENTED();
  return false;
}

std::string AccountSettingSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string AccountSettingSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.account_setting().name();
}

}  // namespace autofill
