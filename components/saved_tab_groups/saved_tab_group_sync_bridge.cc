// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/base/deletion_origin.h"
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

namespace tab_groups {
namespace {

// Discard orphaned tabs after 30 days if the associated group cannot be found.
constexpr base::TimeDelta kDiscardOrphanedTabsThreshold = base::Days(30);

std::unique_ptr<syncer::EntityData> CreateEntityData(
    sync_pb::SavedTabGroupSpecifics specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specific.guid();
  entity_data->specifics.mutable_saved_tab_group()->Swap(&specific);
  return entity_data;
}

std::optional<size_t> GroupPositionFromSpecifics(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  // In v1 we always set tab group position even if the proto is not set, which
  // gives a default position of 0. In v2 we leave the position unset if the
  // proto is not set for unpinned tab groups.
  if (!IsTabGroupsSaveUIUpdateEnabled()) {
    return specifics.group().position();
  }
  if (specifics.group().has_pinned_position()) {
    return specifics.group().pinned_position();
  }
  return std::nullopt;
}

base::Time TimeFromWindowsEpochMicros(int64_t time_windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(time_windows_epoch_micros));
}

tab_groups::TabGroupColorId SyncColorToTabGroupColor(
    const sync_pb::SavedTabGroup::SavedTabGroupColor color) {
  switch (color) {
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY:
      return tab_groups::TabGroupColorId::kGrey;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE:
      return tab_groups::TabGroupColorId::kBlue;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_RED:
      return tab_groups::TabGroupColorId::kRed;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_YELLOW:
      return tab_groups::TabGroupColorId::kYellow;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREEN:
      return tab_groups::TabGroupColorId::kGreen;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PINK:
      return tab_groups::TabGroupColorId::kPink;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PURPLE:
      return tab_groups::TabGroupColorId::kPurple;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_CYAN:
      return tab_groups::TabGroupColorId::kCyan;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_ORANGE:
      return tab_groups::TabGroupColorId::kOrange;
    case sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_UNSPECIFIED:
      return tab_groups::TabGroupColorId::kGrey;
  }
}

sync_pb::SavedTabGroup_SavedTabGroupColor TabGroupColorToSyncColor(
    const tab_groups::TabGroupColorId color) {
  switch (color) {
    case tab_groups::TabGroupColorId::kGrey:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY;
    case tab_groups::TabGroupColorId::kBlue:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_BLUE;
    case tab_groups::TabGroupColorId::kRed:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_RED;
    case tab_groups::TabGroupColorId::kYellow:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_YELLOW;
    case tab_groups::TabGroupColorId::kGreen:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREEN;
    case tab_groups::TabGroupColorId::kPink:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PINK;
    case tab_groups::TabGroupColorId::kPurple:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_PURPLE;
    case tab_groups::TabGroupColorId::kCyan:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_CYAN;
    case tab_groups::TabGroupColorId::kOrange:
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_ORANGE;
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return sync_pb::SavedTabGroup::SAVED_TAB_GROUP_COLOR_GREY;
  }

  NOTREACHED_IN_MIGRATION() << "No known conversion for the supplied color.";
}

SavedTabGroup SpecificsToSavedTabGroup(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  CHECK(specific.has_group());

  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specific.group().color());
  const std::u16string& title = base::UTF8ToUTF16(specific.group().title());
  std::optional<size_t> position = GroupPositionFromSpecifics(specific);
  const base::Uuid guid = base::Uuid::ParseLowercase(specific.guid());
  const base::Time creation_time =
      TimeFromWindowsEpochMicros(specific.creation_time_windows_epoch_micros());
  const base::Time update_time =
      TimeFromWindowsEpochMicros(specific.update_time_windows_epoch_micros());
  SavedTabGroup group = SavedTabGroup(title, color, {}, position, guid,
                                      std::nullopt, creation_time);
  group.SetUpdateTimeWindowsEpochMicros(update_time);

  return group;
}

sync_pb::SavedTabGroupSpecifics SavedTabGroupToSpecifics(
    const SavedTabGroup& group) {
  sync_pb::SavedTabGroupSpecifics pb_specific;
  pb_specific.set_guid(group.saved_guid().AsLowercaseString());
  pb_specific.set_creation_time_windows_epoch_micros(
      group.creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific.set_update_time_windows_epoch_micros(
      group.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SavedTabGroup* pb_group = pb_specific.mutable_group();
  pb_group->set_color(TabGroupColorToSyncColor(group.color()));
  pb_group->set_title(base::UTF16ToUTF8(group.title()));

  if (group.position().has_value()) {
    if (IsTabGroupsSaveUIUpdateEnabled()) {
      pb_group->set_pinned_position(group.position().value());
    } else {
      pb_group->set_position(group.position().value());
    }
  }
  // Note: When adding a new syncable field, also update IsSyncEquivalent().

  return pb_specific;
}

SavedTabGroupTab SpecificsToSavedTabGroupTab(
    const sync_pb::SavedTabGroupSpecifics& specific) {
  CHECK(specific.has_tab());

  const base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.creation_time_windows_epoch_micros()));
  const base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specific.update_time_windows_epoch_micros()));

  return SavedTabGroupTab(
      GURL(specific.tab().url()), base::UTF8ToUTF16(specific.tab().title()),
      base::Uuid::ParseLowercase(specific.tab().group_guid()),
      specific.tab().position(), base::Uuid::ParseLowercase(specific.guid()),
      std::nullopt, creation_time, update_time);
}

sync_pb::SavedTabGroupSpecifics SavedTabGroupTabToSpecifics(
    const SavedTabGroupTab& tab) {
  sync_pb::SavedTabGroupSpecifics pb_specific;
  pb_specific.set_guid(tab.saved_tab_guid().AsLowercaseString());
  pb_specific.set_creation_time_windows_epoch_micros(
      tab.creation_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  pb_specific.set_update_time_windows_epoch_micros(
      tab.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SavedTabGroupTab* pb_tab = pb_specific.mutable_tab();
  pb_tab->set_url(tab.url().spec());
  pb_tab->set_group_guid(tab.saved_group_guid().AsLowercaseString());
  pb_tab->set_title(base::UTF16ToUTF8(tab.title()));
  pb_tab->set_position(tab.position().value());
  // Note: When adding a new syncable field, also update IsSyncEquivalent().

  return pb_specific;
}

std::vector<sync_pb::SavedTabGroupSpecifics> LoadStoredEntries(
    std::vector<sync_pb::SavedTabGroupSpecifics> stored_entries,
    SavedTabGroupModel* model) {
  std::vector<SavedTabGroup> groups;
  std::unordered_set<std::string> group_guids;

  // `stored_entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (const sync_pb::SavedTabGroupSpecifics& proto : stored_entries) {
    if (proto.has_group()) {
      groups.emplace_back(SpecificsToSavedTabGroup(proto));
      group_guids.emplace(proto.guid());
    }
  }

  // Parse tabs and find tabs missing groups.
  std::vector<sync_pb::SavedTabGroupSpecifics> tabs_missing_groups;
  std::vector<SavedTabGroupTab> tabs;
  for (sync_pb::SavedTabGroupSpecifics& proto : stored_entries) {
    if (proto.has_group()) {
      continue;
    }
    if (group_guids.contains(proto.tab().group_guid())) {
      tabs.emplace_back(SpecificsToSavedTabGroupTab(proto));
      continue;
    }
    tabs_missing_groups.push_back(std::move(proto));
  }

  model->LoadStoredEntries(std::move(groups), std::move(tabs));
  return tabs_missing_groups;
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

std::optional<syncer::ModelError> SavedTabGroupSyncBridge::MergeFullSyncData(
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
      SendToSync(SavedTabGroupTabToSpecifics(tab), metadata_change_list.get());
    }

    if (synced_items.count(group.saved_guid().AsLowercaseString()))
      continue;
    SendToSync(SavedTabGroupToSpecifics(group), metadata_change_list.get());
  }

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::optional<syncer::ModelError>
SavedTabGroupSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  std::vector<std::string> deleted_entities;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        deleted_entities.push_back(change->storage_key());
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

  // Process deleted entities last. This is done for consistency. Since
  // `entity_changes` is not guaranteed to be in order, it is possible that a
  // user could add or remove tabs in a way that puts the group in an empty
  // state. This will unintentionally delete the group and drop any additional
  // add / update messages. By processing deletes last, we can give the groups
  // an opportunity to resolve themselves before they become empty.
  for (const std::string& entity : deleted_entities) {
    DeleteDataFromLocalStorage(base::Uuid::ParseLowercase(entity),
                               write_batch.get());
  }

  ResolveTabsMissingGroups(write_batch.get());

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

void SavedTabGroupSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Close all the groups locally. They should still exist in sync server.
  std::vector<base::Uuid> group_ids;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    model_->RemovedFromSync(group.saved_guid());
  }

  // Wipe out all the local data.
  store_->DeleteAllDataAndMetadata(base::DoNothing());
}

std::string SavedTabGroupSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.saved_tab_group().guid();
}

std::string SavedTabGroupSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

void SavedTabGroupSyncBridge::GetDataForCommit(StorageKeyList storage_keys,
                                               DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& guid : storage_keys) {
    base::Uuid parsed_guid = base::Uuid::ParseLowercase(guid);
    for (const SavedTabGroup& group : model_->saved_tab_groups()) {
      if (group.saved_guid() == parsed_guid) {
        AddEntryToBatch(batch.get(), SavedTabGroupToSpecifics(group));
        break;
      }

      if (group.ContainsTab(parsed_guid)) {
        const SavedTabGroupTab& tab =
            group.saved_tabs()[group.GetIndexOfTab(parsed_guid).value()];
        AddEntryToBatch(batch.get(), SavedTabGroupTabToSpecifics(tab));
        break;
      }
    }
  }

  std::move(callback).Run(std::move(batch));
}

void SavedTabGroupSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    AddEntryToBatch(batch.get(), SavedTabGroupToSpecifics(group));
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      AddEntryToBatch(batch.get(), SavedTabGroupTabToSpecifics(tab));
    }
  }

  std::move(callback).Run(std::move(batch));
}

// SavedTabGroupModelObserver
void SavedTabGroupSyncBridge::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* group = model_->Get(guid);
  DCHECK(group);

  int index = model_->GetIndexOf(guid).value();
  sync_pb::SavedTabGroupSpecifics group_specific =
      SavedTabGroupToSpecifics(*group);
  group_specific.mutable_group()->set_position(index);

  UpsertEntitySpecific(std::move(group_specific), write_batch.get());
  for (size_t i = 0; i < group->saved_tabs().size(); ++i) {
    sync_pb::SavedTabGroupSpecifics tab_specific =
        SavedTabGroupTabToSpecifics(group->saved_tabs()[i]);
    tab_specific.mutable_tab()->set_position(i);
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

  // Intentionally only remove the group (creating orphaned tabs in the
  // process), so other devices with the group open in the Tabstrip can react to
  // the deletion appropriately (i.e. We do not have to determine if a tab
  // deletion was part of a group deletion).
  RemoveEntitySpecific(removed_group->saved_guid(), write_batch.get());

  // Keep track of the newly orphaned tabs since their group no longer exists.
  for (const SavedTabGroupTab& tab : removed_group->saved_tabs()) {
    tabs_missing_groups_.emplace_back(SavedTabGroupTabToSpecifics(tab));
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));

  // Update the ModelTypeStore (local storage) and sync with the new positions
  // of all the groups after a remove has occurred so the positions are
  // preserved on browser restart. See crbug/1462443.
  SavedTabGroupReorderedLocally();
}

void SavedTabGroupSyncBridge::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_->Get(group_guid);
  DCHECK(group);

  if (tab_guid.has_value()) {
    if (!group->ContainsTab(tab_guid.value())) {
      RemoveEntitySpecific(tab_guid.value(), write_batch.get());
    } else {
      int tab_index = group->GetIndexOfTab(tab_guid.value()).value();
      UpsertEntitySpecific(
          SavedTabGroupTabToSpecifics(group->saved_tabs()[tab_index]),
          write_batch.get());
    }
  } else {
    UpsertEntitySpecific(SavedTabGroupToSpecifics(*group), write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupTabsReorderedLocally(
    const base::Uuid& group_guid) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_->Get(group_guid);
  DCHECK(group);

  for (const SavedTabGroupTab& tab : group->saved_tabs()) {
    UpsertEntitySpecific(SavedTabGroupTabToSpecifics(tab), write_batch.get());
  }

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

void SavedTabGroupSyncBridge::SavedTabGroupReorderedLocally() {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    UpsertEntitySpecific(SavedTabGroupToSpecifics(group), write_batch.get());
  }

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

// static
SavedTabGroup SavedTabGroupSyncBridge::SpecificsToSavedTabGroupForTest(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  return SpecificsToSavedTabGroup(specifics);
}

// static
sync_pb::SavedTabGroupSpecifics
SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(
    const SavedTabGroup& group) {
  return SavedTabGroupToSpecifics(group);
}

// static
SavedTabGroupTab SavedTabGroupSyncBridge::SpecificsToSavedTabGroupTabForTest(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  return SpecificsToSavedTabGroupTab(specifics);
}

// static
sync_pb::SavedTabGroupSpecifics
SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
    const SavedTabGroupTab& tab) {
  return SavedTabGroupTabToSpecifics(tab);
}

void SavedTabGroupSyncBridge::UpsertEntitySpecific(
    const sync_pb::SavedTabGroupSpecifics& specific,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->WriteData(specific.guid(), specific.SerializeAsString());
  SendToSync(std::move(specific), write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::RemoveEntitySpecific(
    const base::Uuid& guid,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());

  if (!change_processor()->IsTrackingMetadata())
    return;

  change_processor()->Delete(guid.AsLowercaseString(),
                             syncer::DeletionOrigin::Unspecified(),
                             write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::AddDataToLocalStorage(
    const sync_pb::SavedTabGroupSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::ModelTypeStore::WriteBatch* write_batch,
    bool notify_sync) {
  base::Uuid group_guid = base::Uuid::ParseLowercase(
      specifics.has_tab() ? specifics.tab().group_guid() : specifics.guid());
  const SavedTabGroup* existing_group = model_->Get(group_guid);

  // Cases where `specifics` is a group.
  if (specifics.has_group()) {
    if (existing_group) {
      // Resolve the conflict by merging the sync and local data. Once
      // finished, write the result to the store and update sync with the new
      // merged result if appropriate.
      existing_group = model_->MergeRemoteGroupMetadata(
          group_guid, base::UTF8ToUTF16(specifics.group().title()),
          SyncColorToTabGroupColor(specifics.group().color()),
          GroupPositionFromSpecifics(specifics),
          TimeFromWindowsEpochMicros(
              specifics.update_time_windows_epoch_micros()));
      sync_pb::SavedTabGroupSpecifics updated_specifics =
          SavedTabGroupToSpecifics(*existing_group);

      // Write result to the store.
      write_batch->WriteData(updated_specifics.guid(),
                             updated_specifics.SerializeAsString());

      // Update sync with the new merged result.
      if (notify_sync)
        SendToSync(std::move(updated_specifics), metadata_change_list);
    } else {
      // We do not have this group. Add the group from sync into local storage.
      write_batch->WriteData(specifics.guid(), specifics.SerializeAsString());
      model_->AddedFromSync(SpecificsToSavedTabGroup(specifics));
    }

    return;
  }

  // Cases where `specifics` is a tab.
  if (specifics.has_tab()) {
    base::Uuid tab_guid = base::Uuid::ParseLowercase(specifics.guid());
    if (existing_group && existing_group->ContainsTab(tab_guid)) {
      // Resolve the conflict by merging the sync and local data. Once finished,
      // write the result to the store and update sync with the new merged
      // result if appropriate.
      const SavedTabGroupTab* merged_tab =
          model_->MergeRemoteTab(SpecificsToSavedTabGroupTab(specifics));
      sync_pb::SavedTabGroupSpecifics merged_entry =
          SavedTabGroupTabToSpecifics(*merged_tab);

      // Write result to the store.
      write_batch->WriteData(merged_entry.guid(),
                             merged_entry.SerializeAsString());

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
      model_->AddTabToGroupFromSync(existing_group->saved_guid(),
                                    SpecificsToSavedTabGroupTab(specifics));
    } else {
      // We reach this case if we were unable to find a group for this tab. This
      // can happen when sync sends the tab data before the group data. In this
      // case, we will store the tabs in case the group comes in later.
      tabs_missing_groups_.emplace_back(std::move(specifics));
    }
  }
}

void SavedTabGroupSyncBridge::DeleteDataFromLocalStorage(
    const base::Uuid& guid,
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());
  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  // TODO(b/336586617): Close tabs on desktop on receiving this event.
  if (model_->Contains(guid)) {
    model_->RemovedFromSync(guid);
    return;
  }

  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (!group.ContainsTab(guid))
      continue;

    model_->RemoveTabFromGroupFromSync(group.saved_guid(), guid);
    return;
  }
}

void SavedTabGroupSyncBridge::ResolveTabsMissingGroups(
    syncer::ModelTypeStore::WriteBatch* write_batch) {
  auto tab_iterator = tabs_missing_groups_.begin();
  while (tab_iterator != tabs_missing_groups_.end()) {
    const SavedTabGroup* group = model_->Get(
        base::Uuid::ParseLowercase(tab_iterator->tab().group_guid()));
    if (!group) {
      base::Time last_update_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(tab_iterator->update_time_windows_epoch_micros()));
      base::Time now = base::Time::Now();

      // Discard the tabs that do not have an associated group and have not been
      // updated within the discard threshold.
      if (now - last_update_time >= kDiscardOrphanedTabsThreshold) {
        RemoveEntitySpecific(base::Uuid::ParseLowercase(tab_iterator->guid()),
                             write_batch);
        tab_iterator = tabs_missing_groups_.erase(tab_iterator);
      } else {
        ++tab_iterator;
      }
    } else {
      write_batch->WriteData(tab_iterator->guid(),
                             tab_iterator->SerializeAsString());
      model_->AddTabToGroupFromSync(group->saved_guid(),
                                    SpecificsToSavedTabGroupTab(*tab_iterator));
      tab_iterator = tabs_missing_groups_.erase(tab_iterator);
    }
  }
}

void SavedTabGroupSyncBridge::AddEntryToBatch(
    syncer::MutableDataBatch* batch,
    sync_pb::SavedTabGroupSpecifics specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(specific));

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;

  batch->Put(name, std::move(entity_data));
}

void SavedTabGroupSyncBridge::SendToSync(
    sync_pb::SavedTabGroupSpecifics specific,
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
    const std::optional<syncer::ModelError>& error,
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
    const std::optional<syncer::ModelError>& error,
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
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "SavedTabGroupSyncBridge::OnReadAllMetadata");
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
  tabs_missing_groups_ =
      LoadStoredEntries(std::move(stored_entries), model_.get());
  observation_.Observe(model_);
}

void SavedTabGroupSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to save metadata."});
    return;
  }

  // TODO(dljames): React to store failures when a save is not successful.
}

}  // namespace tab_groups
