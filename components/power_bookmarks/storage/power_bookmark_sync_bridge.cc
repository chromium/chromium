// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"

namespace power_bookmarks {

namespace {
std::unique_ptr<syncer::DataBatch> ConvertPowersToSyncData(
    const std::vector<std::unique_ptr<Power>>& powers) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& power : powers) {
    std::string guid = power->guid_string();
    auto entity_data = std::make_unique<syncer::EntityData>();
    entity_data->name = guid;
    power->ToPowerBookmarkSpecifics(
        entity_data->specifics.mutable_power_bookmark());
    batch->Put(guid, std::move(entity_data));
  }
  return batch;
}
}  // namespace

PowerBookmarkSyncBridge::PowerBookmarkSyncBridge(
    PowerBookmarkSyncMetadataDatabase* meta_db,
    Delegate* delegate,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      meta_db_(meta_db),
      delegate_(delegate) {}

PowerBookmarkSyncBridge::~PowerBookmarkSyncBridge() = default;

void PowerBookmarkSyncBridge::Init() {
  std::unique_ptr<syncer::MetadataBatch> batch = meta_db_->GetAllSyncMetadata();
  if (batch) {
    initialized_ = true;
    change_processor()->ModelReadyToSync(std::move(batch));
  } else {
    change_processor()->ReportError({FROM_HERE, "Failed to load metadata"});
  }
}

std::unique_ptr<syncer::MetadataChangeList>
PowerBookmarkSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> PowerBookmarkSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return ApplyChanges(std::move(metadata_change_list), entity_changes,
                      /*is_initial_merge=*/true);
}

std::optional<syncer::ModelError>
PowerBookmarkSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return ApplyChanges(std::move(metadata_change_list), entity_changes,
                      /*is_initial_merge=*/false);
}

std::string PowerBookmarkSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.power_bookmark().guid();
}

std::string PowerBookmarkSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::unique_ptr<syncer::DataBatch> PowerBookmarkSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return ConvertPowersToSyncData(delegate_->GetPowersForGUIDs(storage_keys));
}

std::unique_ptr<syncer::DataBatch>
PowerBookmarkSyncBridge::GetAllDataForDebugging() {
  return ConvertPowersToSyncData(delegate_->GetAllPowers());
}

void PowerBookmarkSyncBridge::SendPowerToSync(const Power& power) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  auto entity_data = std::make_unique<syncer::EntityData>();
  power.ToPowerBookmarkSpecifics(
      entity_data->specifics.mutable_power_bookmark());
  entity_data->name = power.guid_string();

  change_processor()->Put(power.guid_string(), std::move(entity_data),
                          CreateMetadataChangeListInTransaction().get());
}

void PowerBookmarkSyncBridge::NotifySyncForDeletion(const std::string& guid) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                             CreateMetadataChangeListInTransaction().get());
}

std::unique_ptr<syncer::MetadataChangeList>
PowerBookmarkSyncBridge::CreateMetadataChangeListInTransaction() {
  // TODO(crbug.com/40247772): Add a DCHECK to make sure this is called inside a
  // transaction.
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      meta_db_, syncer::POWER_BOOKMARK,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> PowerBookmarkSyncBridge::ApplyChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList& entity_changes,
    bool is_initial_merge) {
  std::set<std::string> synced_entries;
  std::unique_ptr<Transaction> transaction = delegate_->BeginTransaction();
  if (!transaction) {
    return syncer::ModelError(
        FROM_HERE, "Failed to begin transaction for PowerBookmarks.");
  }

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE:
        if (!delegate_->CreateOrMergePowerFromSync(*std::make_unique<Power>(
                change->data().specifics.power_bookmark()))) {
          return syncer::ModelError(
              FROM_HERE, "Failed to merge local powers for PowerBookmarks.");
        }
        if (is_initial_merge) {
          synced_entries.insert(change->storage_key());
        }
        break;
      case syncer::EntityChange::ACTION_DELETE:
        if (!delegate_->DeletePowerFromSync(change->storage_key())) {
          return syncer::ModelError(
              FROM_HERE, "Failed to delete local powers for PowerBookmarks.");
        }
        break;
    }
  }

  if (is_initial_merge) {
    // Send local only powers to sync.
    for (const std::unique_ptr<Power>& power : delegate_->GetAllPowers()) {
      if (synced_entries.count(power->guid_string()) == 0) {
        SendPowerToSync(*power);
      }
    }
  }

  std::unique_ptr<syncer::MetadataChangeList> power_bookmark_change_list =
      CreateMetadataChangeListInTransaction();
  static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
      ->TransferChangesTo(power_bookmark_change_list.get());

  if (!transaction->Commit()) {
    return syncer::ModelError(
        FROM_HERE, "Failed to commit transaction for PowerBookmarks.");
  }

  delegate_->NotifyPowersChanged();
  return {};
}

void PowerBookmarkSyncBridge::ReportError(const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

}  // namespace power_bookmarks
