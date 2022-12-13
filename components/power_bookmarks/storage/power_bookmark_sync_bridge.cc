// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"

#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

namespace power_bookmarks {

namespace {
void WritePowersToSyncData(const std::vector<std::unique_ptr<Power>>& powers,
                           PowerBookmarkSyncBridge::DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& power : powers) {
    std::string guid = power->guid().AsLowercaseString();
    auto entity_data = std::make_unique<syncer::EntityData>();
    entity_data->name = guid;
    power->ToPowerBookmarkSpecifics(
        entity_data->specifics.mutable_power_bookmark());
    batch->Put(guid, std::move(entity_data));
  }
  std::move(callback).Run(std::move(batch));
}
}  // namespace

PowerBookmarkSyncBridge::PowerBookmarkSyncBridge(
    PowerBookmarkSyncMetadataDatabase* meta_db,
    Delegate* delegate,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      meta_db_(meta_db),
      delegate_(delegate) {}

PowerBookmarkSyncBridge::~PowerBookmarkSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PowerBookmarkSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      meta_db_, syncer::POWER_BOOKMARK,
      base::BindRepeating(&syncer::ModelTypeChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

absl::optional<syncer::ModelError> PowerBookmarkSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return {};
}

absl::optional<syncer::ModelError> PowerBookmarkSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return {};
}

std::string PowerBookmarkSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.power_bookmark().guid();
}

std::string PowerBookmarkSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

void PowerBookmarkSyncBridge::GetData(StorageKeyList storage_keys,
                                      DataCallback callback) {
  WritePowersToSyncData(delegate_->GetPowersForGUIDs(storage_keys),
                        std::move(callback));
}

void PowerBookmarkSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  WritePowersToSyncData(delegate_->GetAllPowers(), std::move(callback));
}

}  // namespace power_bookmarks
