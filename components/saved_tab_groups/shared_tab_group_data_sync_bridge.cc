// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"

namespace tab_groups {

namespace {

tab_groups::TabGroupColorId SyncColorToTabGroupColor(
    const sync_pb::SharedTabGroup::Color color) {
  switch (color) {
    case sync_pb::SharedTabGroup::GREY:
      return tab_groups::TabGroupColorId::kGrey;
    case sync_pb::SharedTabGroup::BLUE:
      return tab_groups::TabGroupColorId::kBlue;
    case sync_pb::SharedTabGroup::RED:
      return tab_groups::TabGroupColorId::kRed;
    case sync_pb::SharedTabGroup::YELLOW:
      return tab_groups::TabGroupColorId::kYellow;
    case sync_pb::SharedTabGroup::GREEN:
      return tab_groups::TabGroupColorId::kGreen;
    case sync_pb::SharedTabGroup::PINK:
      return tab_groups::TabGroupColorId::kPink;
    case sync_pb::SharedTabGroup::PURPLE:
      return tab_groups::TabGroupColorId::kPurple;
    case sync_pb::SharedTabGroup::CYAN:
      return tab_groups::TabGroupColorId::kCyan;
    case sync_pb::SharedTabGroup::ORANGE:
      return tab_groups::TabGroupColorId::kOrange;
    case sync_pb::SharedTabGroup::UNSPECIFIED:
      return tab_groups::TabGroupColorId::kGrey;
  }
}

sync_pb::SharedTabGroup_Color TabGroupColorToSyncColor(
    const tab_groups::TabGroupColorId color) {
  switch (color) {
    case tab_groups::TabGroupColorId::kGrey:
      return sync_pb::SharedTabGroup::GREY;
    case tab_groups::TabGroupColorId::kBlue:
      return sync_pb::SharedTabGroup::BLUE;
    case tab_groups::TabGroupColorId::kRed:
      return sync_pb::SharedTabGroup::RED;
    case tab_groups::TabGroupColorId::kYellow:
      return sync_pb::SharedTabGroup::YELLOW;
    case tab_groups::TabGroupColorId::kGreen:
      return sync_pb::SharedTabGroup::GREEN;
    case tab_groups::TabGroupColorId::kPink:
      return sync_pb::SharedTabGroup::PINK;
    case tab_groups::TabGroupColorId::kPurple:
      return sync_pb::SharedTabGroup::PURPLE;
    case tab_groups::TabGroupColorId::kCyan:
      return sync_pb::SharedTabGroup::CYAN;
    case tab_groups::TabGroupColorId::kOrange:
      return sync_pb::SharedTabGroup::ORANGE;
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return sync_pb::SharedTabGroup::GREY;
  }

  NOTREACHED_IN_MIGRATION() << "No known conversion for the supplied color.";
}

base::Time TimeFromWindowsEpochMicros(int64_t time_windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(time_windows_epoch_micros));
}

sync_pb::SharedTabGroupDataSpecifics SharedTabGroupToSpecifics(
    const SavedTabGroup& group) {
  // TODO(crbug.com/319521964): check if the `group` is shared tab group.
  sync_pb::SharedTabGroupDataSpecifics pb_specifics;
  pb_specifics.set_guid(group.saved_guid().AsLowercaseString());
  pb_specifics.set_update_time_windows_epoch_micros(
      group.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SharedTabGroup* pb_group = pb_specifics.mutable_tab_group();
  pb_group->set_color(TabGroupColorToSyncColor(group.color()));
  pb_group->set_title(base::UTF16ToUTF8(group.title()));
  return pb_specifics;
}

SavedTabGroup SpecificsToSharedTabGroup(
    const sync_pb::SharedTabGroupDataSpecifics& specifics) {
  CHECK(specifics.has_tab_group());

  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specifics.tab_group().color());
  const std::u16string& title =
      base::UTF8ToUTF16(specifics.tab_group().title());
  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());

  // GUID must be checked before this method is called.
  CHECK(guid.is_valid());

  const base::Time update_time =
      TimeFromWindowsEpochMicros(specifics.update_time_windows_epoch_micros());

  SavedTabGroup group(title, color, /*urls=*/{}, /*position=*/std::nullopt,
                      guid, /*local_group_id=*/std::nullopt,
                      /*creator_cache_guid=*/std::nullopt,
                      /*last_updater_cache_guid=*/std::nullopt,
                      /*created_before_syncing_tab_groups=*/false,
                      /*creation_time_windows_epoch_micros=*/std::nullopt,
                      update_time);
  return group;
}

}  // namespace

SharedTabGroupDataSyncBridge::SharedTabGroupDataSyncBridge(
    SavedTabGroupModel* model,
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    PrefService* pref_service)
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
  // This data type does not have local data and hence there is nothing to
  // merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  std::vector<std::string> deleted_entities;

  std::vector<std::unique_ptr<syncer::EntityChange>> tab_updates;
  for (std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        deleted_entities.push_back(change->storage_key());
        break;
      }
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        if (change->data().specifics.shared_tab_group_data().has_tab_group()) {
          AddGroupToLocalStorage(
              change->data().specifics.shared_tab_group_data(),
              metadata_change_list.get(), write_batch.get());
        } else if (change->data().specifics.shared_tab_group_data().has_tab()) {
          // Postpone tab updates until all remote groups are added.
          tab_updates.push_back(std::move(change));
        }
        // Ignore entities not having a tab or a group.
        break;
      }
    }
  }

  // Process deleted entities last. This is done for consistency. Since
  // `entity_changes` is not guaranteed to be in order, it is possible that a
  // user could add or remove tabs in a way that puts the group in an empty
  // state. This will unintentionally delete the group and drop any additional
  // add / update messages. By processing deletes last, we can give the groups
  // an opportunity to resolve themselves before they become empty.
  for (const std::string& entity : deleted_entities) {
    DeleteDataFromLocalStorage(entity, write_batch.get());
  }

  // Process tab updates after applying deletions so that tab updates having
  // deleted groups will be stored to `tabs_missing_groups_`.
  for (const std::unique_ptr<syncer::EntityChange>& change : tab_updates) {
    AddTabToLocalStorage(change->data().specifics.shared_tab_group_data(),
                         metadata_change_list.get(), write_batch.get());
  }

  // TODO(crbug.com/319521964): resolve and handle tabs missing groups later.
  // ResolveTabsMissingGroups(write_batch.get());

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

void SharedTabGroupDataSyncBridge::GetDataForCommit(StorageKeyList storage_keys,
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
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // TODO(crbug.com/319521964): Process result.
}

void SharedTabGroupDataSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to store data."});
  }
}

void SharedTabGroupDataSyncBridge::AddGroupToLocalStorage(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  base::Uuid group_guid = base::Uuid::ParseLowercase(specifics.guid());
  if (!group_guid.is_valid()) {
    // Ignore remote updates having invalid data.
    return;
  }

  CHECK(specifics.has_tab_group());

  if (!model_->Contains(group_guid)) {
    // This is a new remotely created group. Add the group from sync into local
    // storage.
    write_batch->WriteData(specifics.guid(), specifics.SerializeAsString());
    model_->AddedFromSync(SpecificsToSharedTabGroup(specifics));
    return;
  }

  // Update the existing group with remote data.
  // TODO(crbug.com/319521964): handle group position properly.
  const SavedTabGroup* existing_group = model_->MergeRemoteGroupMetadata(
      group_guid, base::UTF8ToUTF16(specifics.tab_group().title()),
      SyncColorToTabGroupColor(specifics.tab_group().color()),
      /*position=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      TimeFromWindowsEpochMicros(specifics.update_time_windows_epoch_micros()));
  CHECK(existing_group);
  sync_pb::SharedTabGroupDataSpecifics updated_specifics =
      SharedTabGroupToSpecifics(*existing_group);

  // Write result to the store.
  // TODO(crbug.com/319521964): introduce an additional layer to store specifics
  // with local-only data.
  write_batch->WriteData(updated_specifics.guid(),
                         updated_specifics.SerializeAsString());
}

void SharedTabGroupDataSyncBridge::AddTabToLocalStorage(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  CHECK(specifics.has_tab());
  NOTIMPLEMENTED();
}

void SharedTabGroupDataSyncBridge::DeleteDataFromLocalStorage(
    const std::string& storage_key,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(storage_key);

  base::Uuid guid = base::Uuid::ParseLowercase(storage_key);
  if (!guid.is_valid()) {
    return;
  }

  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  if (model_->Contains(guid)) {
    model_->RemovedFromSync(guid);
    return;
  }

  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (!group.ContainsTab(guid)) {
      continue;
    }

    model_->RemoveTabFromGroupFromSync(group.saved_guid(), guid);
    return;
  }
}

}  // namespace tab_groups
