// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"

namespace commerce {

ProductSpecificationsSyncBridge::ProductSpecificationsSyncBridge(
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {}

ProductSpecificationsSyncBridge::~ProductSpecificationsSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
ProductSpecificationsSyncBridge::CreateMetadataChangeList() {
  // TODO(b/329519916) implement
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO(b/329519487) implement
  NOTIMPLEMENTED();
  return std::nullopt;
}
std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO(b/329519488) implement
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::string ProductSpecificationsSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  // TODO(b/329520372) implement
  NOTIMPLEMENTED();
  return "";
}
std::string ProductSpecificationsSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  // TODO(b/329520414) implement
  NOTIMPLEMENTED();
  return "";
}

void ProductSpecificationsSyncBridge::GetData(StorageKeyList storage_keys,
                                              DataCallback callback) {
  // TODO(b/329520479) implement
  NOTIMPLEMENTED();
}

void ProductSpecificationsSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  // TODO(b/329520107) implement
  NOTIMPLEMENTED();
}

}  // namespace commerce
