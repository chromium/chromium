// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"

#include "base/logging.h"

namespace {
constexpr base::TimeDelta discard_orphaned_tabs_threshold =
    base::Microseconds(base::Time::kMicrosecondsPerDay * 90);

std::unique_ptr<syncer::EntityData> CreateEntityData(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specific->guid();
  entity_data->specifics.set_allocated_saved_tab_group(specific.release());
  return entity_data;
}

}  // anonymous namespace

SavedTabGroupSyncBridge::SavedTabGroupSyncBridge(
    SavedTabGroupModel* model,
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)), model_(model) {
  DCHECK(model_);
  std::move(create_store_callback)
      .Run(syncer::SAVED_TAB_GROUP,
           base::BindOnce(&SavedTabGroupSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SavedTabGroupSyncBridge::~SavedTabGroupSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
SavedTabGroupSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> SavedTabGroupSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  std::set<std::string> synced_items;

  // Merge sync to local data.
  for (const auto& change : entity_changes) {
    synced_items.insert(change->storage_key());
    AddDataToLocalStorage(std::move(change->data().specifics.saved_tab_group()),
                          metadata_change_list.get(), write_batch.get(),
                          /*notify_sync=*/true);
  }

  ResolveTabsMissingGroups(write_batch.get());

  // Update sync with any locally stored data not currently stored in sync.
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      if (synced_items.count(tab.saved_tab_guid().AsLowercaseString()))
        continue;
      SendToSync(tab.ToSpecifics(), metadata_change_list.get());
    }

    if (synced_items.count(group.saved_guid().AsLowercaseString()))
      continue;
    SendToSync(group.ToSpecifics(), metadata_change_list.get());
  }

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

absl::optional<syncer::ModelError> SavedTabGroupSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        DeleteDataFromLocalStorage(
            base::GUID::ParseLowercase(change->storage_key()),
            write_batch.get());
        break;
      }
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        AddDataToLocalStorage(
            std::move(change->data().specifics.saved_tab_group()),
            metadata_change_list.get(), write_batch.get(),
            /*notify_sync=*/false);
        break;
      }
    }
  }

  ResolveTabsMissingGroups(write_batch.get());

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::string SavedTabGroupSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.saved_tab_group().guid();
}

std::string SavedTabGroupSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

void SavedTabGroupSyncBridge::GetData(StorageKeyList storage_keys,
                                      DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& guid : storage_keys) {
    base::GUID parsed_guid = base::GUID::ParseLowercase(guid);
    for (const SavedTabGroup& group : model_->saved_tab_groups()) {
      if (group.saved_guid() == parsed_guid) {
        AddEntryToBatch(batch.get(), group.ToSpecifics());
        break;
      }

      if (group.ContainsTab(parsed_guid)) {
        const SavedTabGroupTab& tab =
            group.saved_tabs()[group.GetIndexOfTab(parsed_guid).value()];
        AddEntryToBatch(batch.get(), tab.ToSpecifics());
        break;
      }
    }
  }

  std::move(callback).Run(std::move(batch));
}

void SavedTabGroupSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    AddEntryToBatch(batch.get(), group.ToSpecifics());
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      AddEntryToBatch(batch.get(), tab.ToSpecifics());
    }
  }

  std::move(callback).Run(std::move(batch));
}

// SavedTabGroupModelObserver
void SavedTabGroupSyncBridge::SavedTabGroupAddedLocally(
    const base::GUID& guid) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* group = model_->Get(guid);
  DCHECK(group);

  int index = model_->GetIndexOf(guid).value();
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> group_specific =
      group->ToSpecifics();
  group_specific->mutable_group()->set_position(index);

  UpsertEntitySpecific(std::move(group_specific), write_batch.get());
  for (size_t i = 0; i < group->saved_tabs().size(); ++i) {
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> tab_specific =
        group->saved_tabs()[i].ToSpecifics();
    tab_specific->mutable_tab()->set_position(i);
    UpsertEntitySpecific(std::move(tab_specific), write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupRemovedLocally(
    const SavedTabGroup* removed_group) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  RemoveEntitySpecific(removed_group->saved_guid(), write_batch.get());
  for (const SavedTabGroupTab& tab : removed_group->saved_tabs())
    RemoveEntitySpecific(tab.saved_tab_guid(), write_batch.get());

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupUpdatedLocally(
    const base::GUID& group_guid,
    const absl::optional<base::GUID>& tab_guid) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* group = model_->Get(group_guid);
  DCHECK(group);

  if (tab_guid.has_value()) {
    if (!group->ContainsTab(tab_guid.value())) {
      RemoveEntitySpecific(tab_guid.value(), write_batch.get());
    } else {
      int tab_index = group->GetIndexOfTab(tab_guid.value()).value();
      UpsertEntitySpecific(group->saved_tabs()[tab_index].ToSpecifics(),
                           write_batch.get());
    }
  } else {
    UpsertEntitySpecific(group->ToSpecifics(), write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupReorderedLocally() {
  // TODO(dljames): Find a more efficient way to only upsert the data that has
  // changed. If a group has changed, update all groups. If a tab has changed,
  // update all tabs in its group.
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    UpsertEntitySpecific(group.ToSpecifics(), write_batch.get());
    for (const SavedTabGroupTab& tab : group.saved_tabs())
      UpsertEntitySpecific(tab.ToSpecifics(), write_batch.get());
  }

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

void SavedTabGroupSyncBridge::UpsertEntitySpecific(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->WriteData(specific->guid(), specific->SerializeAsString());
  SendToSync(std::move(specific), write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::RemoveEntitySpecific(
    const base::GUID& guid,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());

  if (!change_processor()->IsTrackingMetadata())
    return;

  change_processor()->Delete(guid.AsLowercaseString(),
                             write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::AddDataToLocalStorage(
    const sync_pb::SavedTabGroupSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::ModelTypeStore::WriteBatch* write_batch,
    bool notify_sync) {
  std::string group_id =
      specifics.has_tab() ? specifics.tab().group_guid() : specifics.guid();
  SavedTabGroup* existing_group =
      model_->Get(base::GUID::ParseLowercase(group_id));

  // Cases where `specifics` is a group.
  if (specifics.has_group()) {
    if (existing_group) {
      // Resolve the conflict by merging the sync and local data. Once
      // finished, write the result to the store and update sync with the new
      // merged result if appropriate.
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> merged_entry =
          model_->MergeGroup(std::move(specifics));

      // Write result to the store.
      write_batch->WriteData(merged_entry->guid(),
                             merged_entry->SerializeAsString());

      // Update sync with the new merged result.
      if (notify_sync)
        SendToSync(std::move(merged_entry), metadata_change_list);
    } else {
      // We do not have this group. Add the group from sync into local storage.
      write_batch->WriteData(specifics.guid(), specifics.SerializeAsString());
      model_->AddedFromSync(SavedTabGroup::FromSpecifics(specifics));
    }

    return;
  }

  // Cases where `specifics` is a tab.
  if (specifics.has_tab()) {
    if (existing_group && existing_group->ContainsTab(
                              base::GUID::ParseLowercase(specifics.guid()))) {
      // Resolve the conflict by merging the sync and local data. Once finished,
      // write the result to the store and update sync with the new merged
      // result if appropriate.
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> merged_entry =
          model_->MergeTab(std::move(specifics));

      // Write result to the store.
      write_batch->WriteData(merged_entry->guid(),
                             merged_entry->SerializeAsString());

      // Update sync with the new merged result.
      if (notify_sync)
        SendToSync(std::move(merged_entry), metadata_change_list);

      return;
    }

    // We write tabs to local storage regardless of the existence of its group
    // in order to recover the tabs in the event the group was not received and
    // a crash / restart occurred.
    write_batch->WriteData(specifics.guid(), specifics.SerializeAsString());

    if (existing_group) {
      // We do not have this tab. Add the tab from sync into local storage.
      model_->AddTabToGroup(existing_group->saved_guid(),
                            SavedTabGroupTab::FromSpecifics(specifics), 0);
    } else {
      // We reach this case if we were unable to find a group for this tab. This
      // can happen when sync sends the tab data before the group data. In this
      // case, we will store the tabs in case the group comes in later.
      // TODO(dljames): Cleanup orphaned tabs after some time if the groups
      // never come in.
      tabs_missing_groups_.emplace_back(std::move(specifics));
    }
  }
}

void SavedTabGroupSyncBridge::DeleteDataFromLocalStorage(
    const base::GUID& guid,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());
  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  if (model_->Contains(guid)) {
    model_->Remove(guid);
    return;
  }

  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (!group.ContainsTab(guid))
      continue;

    model_->RemoveTabFromGroup(group.saved_guid(), guid);
    return;
  }
}

void SavedTabGroupSyncBridge::ResolveTabsMissingGroups(
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  auto tab_iterator = tabs_missing_groups_.begin();
  while (tab_iterator != tabs_missing_groups_.end()) {
    SavedTabGroup* group = model_->Get(
        base::GUID::ParseLowercase(tab_iterator->tab().group_guid()));
    if (!group) {
      base::Time last_update_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(tab_iterator->update_time_windows_epoch_micros()));
      base::Time now = base::Time::Now();

      // Discard orphaned tabs that have not been updated for 90 days.
      if (now - last_update_time >= discard_orphaned_tabs_threshold) {
        RemoveEntitySpecific(base::GUID::ParseLowercase(tab_iterator->guid()),
                             write_batch);
        tab_iterator = tabs_missing_groups_.erase(tab_iterator);
      } else {
        ++tab_iterator;
      }
    } else {
      write_batch->WriteData(tab_iterator->guid(),
                             tab_iterator->SerializeAsString());
      model_->AddTabToGroup(group->saved_guid(),
                            SavedTabGroupTab::FromSpecifics(*tab_iterator), 0);
      tab_iterator = tabs_missing_groups_.erase(tab_iterator);
    }
  }
}

void SavedTabGroupSyncBridge::AddEntryToBatch(
    syncer::MutableDataBatch* batch,
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(specific));

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;

  batch->Put(name, std::move(entity_data));
}

void SavedTabGroupSyncBridge::SendToSync(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specific,
    syncer::MetadataChangeList* metadata_change_list) {
  DCHECK(metadata_change_list);
  if (!change_processor()->IsTrackingMetadata())
    return;

  auto entity_data = CreateEntityData(std::move(specific));

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;
  change_processor()->Put(name, std::move(entity_data), metadata_change_list);
}

void SavedTabGroupSyncBridge::OnStoreCreated(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseLoad,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::OnDatabaseLoad(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_->ReadAllMetadata(
      base::BindOnce(&SavedTabGroupSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entries)));
}

void SavedTabGroupSyncBridge::OnReadAllMetadata(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to read metadata."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  std::vector<sync_pb::SavedTabGroupSpecifics> stored_entries;
  stored_entries.reserve(entries->size());

  for (const syncer::ModelTypeStore::Record& r : *entries) {
    sync_pb::SavedTabGroupSpecifics proto;
    if (!proto.ParseFromString(r.value))
      continue;
    stored_entries.emplace_back(std::move(proto));
  }

  // Update `model_` with any data stored in local storage except for orphaned
  // tabs. Orphaned tabs will be returned and added to `tabs_missing_groups_` in
  // case their missing group ever arrives.
  tabs_missing_groups_ = model_->LoadStoredEntries(std::move(stored_entries));
  observation_.Observe(model_);
}

void SavedTabGroupSyncBridge::OnDatabaseSave(
    const absl::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to save metadata."});
    return;
  }

  // TODO(dljames): React to store failures when a save is not successful.
}
