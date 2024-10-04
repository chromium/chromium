// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"

#include <algorithm>
#include <iterator>
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
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_proto_conversions.h"
#include "components/saved_tab_groups/internal/stats.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/proto/saved_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"

namespace tab_groups {
namespace {

// Time period for orphaned tabs/groups to live till. once this threshold is
// passed, on the next merge, they will be deleted.
constexpr base::TimeDelta kOrphanedObjectDiscardThreshold = base::Days(30);

bool IsValidSpecifics(const sync_pb::SavedTabGroupSpecifics& specifics) {
  // A valid specifics should have at least a guid and be either a group or a
  // tab.
  return specifics.has_guid() && (specifics.has_tab() || specifics.has_group());
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    sync_pb::SavedTabGroupSpecifics specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specific.guid();
  entity_data->specifics.mutable_saved_tab_group()->Swap(&specific);
  return entity_data;
}

base::Time TimeFromWindowsEpochMicros(int64_t time_windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(time_windows_epoch_micros));
}

std::vector<proto::SavedTabGroupData> LoadStoredEntries(
    std::vector<proto::SavedTabGroupData> stored_entries,
    SyncBridgeTabGroupModelWrapper* model_wrapper) {
  std::vector<SavedTabGroup> groups;
  std::unordered_set<std::string> group_guids;

  // `stored_entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (const proto::SavedTabGroupData& proto : stored_entries) {
    if (proto.specifics().has_group()) {
      groups.emplace_back(DataToSavedTabGroup(proto));
      group_guids.emplace(proto.specifics().guid());
    }
  }

  // Parse tabs and find tabs missing groups.
  std::vector<proto::SavedTabGroupData> tabs_missing_groups;
  std::vector<SavedTabGroupTab> tabs;
  for (const proto::SavedTabGroupData& proto : stored_entries) {
    if (proto.specifics().has_group()) {
      continue;
    }
    if (group_guids.contains(proto.specifics().tab().group_guid())) {
      tabs.emplace_back(DataToSavedTabGroupTab(proto));
      continue;
    }
    tabs_missing_groups.push_back(std::move(proto));
  }

  model_wrapper->Initialize(std::move(groups), std::move(tabs));
  return tabs_missing_groups;
}

// Returns the index of the group with `group_id`. The group must exist and be
// present in `groups`.
size_t CalculateIndexOfGroup(const std::vector<const SavedTabGroup*>& groups,
                             const base::Uuid& group_id) {
  auto iter =
      base::ranges::find_if(groups, [&group_id](const SavedTabGroup* group) {
        return group->saved_guid() == group_id;
      });
  CHECK(iter != groups.end());
  return std::distance(groups.begin(), iter);
}

}  // anonymous namespace

SavedTabGroupSyncBridge::SavedTabGroupSyncBridge(
    SyncBridgeTabGroupModelWrapper* model_wrapper,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    PrefService* pref_service)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      model_wrapper_(model_wrapper),
      pref_service_(pref_service) {
  CHECK(model_wrapper_);
  CHECK(pref_service_);
  std::move(create_store_callback)
      .Run(syncer::SAVED_TAB_GROUP,
           base::BindOnce(&SavedTabGroupSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SavedTabGroupSyncBridge::~SavedTabGroupSyncBridge() = default;

void SavedTabGroupSyncBridge::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request) {}

std::unique_ptr<syncer::MetadataChangeList>
SavedTabGroupSyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> SavedTabGroupSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  std::set<std::string> synced_items;

  // MergeFullSyncData is the first command called when the user signs in/or
  // turns sync on. When this happens, a cache guid  will be added to the
  // metadata of the change processor. This cache guid should be used on all
  // groups and tabs that previously didnt have a cache guid attached.
  CHECK(change_processor()->IsTrackingMetadata());
  UpdateLocalCacheGuidForGroups(write_batch.get());

  // Merge sync to local data.
  for (const auto& change : entity_changes) {
    synced_items.insert(change->storage_key());
    AddDataToLocalStorage(std::move(change->data().specifics.saved_tab_group()),
                          metadata_change_list.get(), write_batch.get(),
                          /*notify_sync=*/true);
  }

  ResolveTabsMissingGroups(write_batch.get());
  ResolveGroupsMissingTabs(write_batch.get());

  // Update sync with any locally stored data not currently stored in sync.
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      if (synced_items.count(tab.saved_tab_guid().AsLowercaseString())) {
        continue;
      }
      SendToSync(SavedTabGroupTabToData(tab).specifics(),
                 metadata_change_list.get());
    }

    if (synced_items.count(group->saved_guid().AsLowercaseString())) {
      continue;
    }
    SendToSync(SavedTabGroupToData(*group).specifics(),
               metadata_change_list.get());
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
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
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
  ResolveGroupsMissingTabs(write_batch.get());

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

syncer::ConflictResolution SavedTabGroupSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const syncer::EntityData& remote_data) const {
  if (remote_data.is_deleted()) {
    return syncer::ConflictResolution::kUseLocal;
  }

  CHECK(IsEntityDataValid(remote_data));
  const sync_pb::SavedTabGroupSpecifics& remote_specifics =
      remote_data.specifics.saved_tab_group();

  // Do a conflict resolution based on last update timestamp.
  base::Uuid guid = base::Uuid::ParseLowercase(remote_specifics.guid());
  base::Time local_timestamp;
  if (remote_specifics.has_group()) {
    if (const SavedTabGroup* group = model_wrapper_->GetGroup(guid)) {
      local_timestamp = group->update_time_windows_epoch_micros();
    }
  } else {
    CHECK(remote_specifics.has_tab());
    base::Uuid group_guid =
        base::Uuid::ParseLowercase(remote_specifics.tab().group_guid());
    const SavedTabGroup* group = model_wrapper_->GetGroup(group_guid);
    if (const SavedTabGroupTab* tab = group ? group->GetTab(guid) : nullptr) {
      local_timestamp = tab->update_time_windows_epoch_micros();
    }
  }

  base::Time remote_timestamp = TimeFromWindowsEpochMicros(
      remote_specifics.update_time_windows_epoch_micros());
  bool local_is_more_recent =
      !local_timestamp.is_null() && local_timestamp > remote_timestamp;

  return local_is_more_recent ? syncer::ConflictResolution::kUseLocal
                              : syncer::ConflictResolution::kUseRemote;
}

void SavedTabGroupSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // Close the local groups that were created before sign-in.
  // They should still exist in sync server.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  write_batch->TakeMetadataChangesFrom(std::move(delete_metadata_change_list));
  std::vector<base::Uuid> groups_to_close_locally;
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    if (group->created_before_syncing_tab_groups()) {
      continue;
    }

    groups_to_close_locally.emplace_back(group->saved_guid());
  }

  for (const base::Uuid& group_id : groups_to_close_locally) {
    // This group should be closed locally. Remove it from model and storage and
    // close all the tabs.
    const SavedTabGroup* group = model_wrapper_->GetGroup(group_id);
    std::vector<base::Uuid> tabs_to_close_locally;
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      tabs_to_close_locally.emplace_back(tab.saved_tab_guid());
    }

    for (const base::Uuid& tab_id : tabs_to_close_locally) {
      model_wrapper_->RemoveTabFromGroup(group_id, tab_id);
      write_batch->DeleteData(tab_id.AsLowercaseString());
    }

    // The group could have been deleted when the last tab was closed, hence
    // double check before calling RemoveGroup().
    if (model_wrapper_->GetGroup(group_id)) {
      model_wrapper_->RemoveGroup(group_id);
    }
    write_batch->DeleteData(group_id.AsLowercaseString());
  }

  // Reset the cache guids for groups and tabs on sign-out.
  UpdateLocalCacheGuidForGroups(write_batch.get());

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

std::string SavedTabGroupSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.saved_tab_group().guid();
}

std::string SavedTabGroupSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::unique_ptr<syncer::DataBatch> SavedTabGroupSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& guid : storage_keys) {
    base::Uuid parsed_guid = base::Uuid::ParseLowercase(guid);
    for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
      if (group->saved_guid() == parsed_guid) {
        AddEntryToBatch(batch.get(), SavedTabGroupToData(*group));
        break;
      }

      if (const SavedTabGroupTab* tab = group->GetTab(parsed_guid)) {
        AddEntryToBatch(batch.get(), SavedTabGroupTabToData(*tab));
        break;
      }
    }
  }

  return batch;
}

std::unique_ptr<syncer::DataBatch>
SavedTabGroupSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    AddEntryToBatch(batch.get(), SavedTabGroupToData(*group));
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      AddEntryToBatch(batch.get(), SavedTabGroupTabToData(tab));
    }
  }

  return batch;
}

bool SavedTabGroupSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  const sync_pb::SavedTabGroupSpecifics& specifics =
      entity_data.specifics.saved_tab_group();
  return specifics.has_group() || specifics.has_tab();
}

// SavedTabGroupModelObserver
void SavedTabGroupSyncBridge::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* group = model_wrapper_->GetGroup(guid);
  CHECK(group);

  int index = CalculateIndexOfGroup(model_wrapper_->GetTabGroups(), guid);
  proto::SavedTabGroupData group_data = SavedTabGroupToData(*group);
  group_data.mutable_specifics()->mutable_group()->set_position(index);

  UpsertEntitySpecific(std::move(group_data), write_batch.get());
  for (size_t i = 0; i < group->saved_tabs().size(); ++i) {
    proto::SavedTabGroupData tab_data =
        SavedTabGroupTabToData(group->saved_tabs()[i]);
    tab_data.mutable_specifics()->mutable_tab()->set_position(i);
    UpsertEntitySpecific(std::move(tab_data), write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // Intentionally only remove the group (creating orphaned tabs in the
  // process), so other devices with the group open in the Tabstrip can react to
  // the deletion appropriately (i.e. We do not have to determine if a tab
  // deletion was part of a group deletion).
  RemoveEntitySpecific(removed_group.saved_guid(), write_batch.get());

  // Keep track of the newly orphaned tabs since their group no longer exists.
  for (const SavedTabGroupTab& tab : removed_group.saved_tabs()) {
    tabs_missing_groups_.emplace_back(SavedTabGroupTabToData(tab));
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));

  // Update the DataTypeStore (local storage) and sync with the new positions
  // of all the groups after a remove has occurred so the positions are
  // preserved on browser restart. See crbug/1462443.
  SavedTabGroupReorderedLocally();
}

void SavedTabGroupSyncBridge::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);

  if (tab_guid.has_value()) {
    if (!group->ContainsTab(tab_guid.value())) {
      RemoveEntitySpecific(tab_guid.value(), write_batch.get());
    } else {
      int tab_index = group->GetIndexOfTab(tab_guid.value()).value();
      UpsertEntitySpecific(
          SavedTabGroupTabToData(group->saved_tabs()[tab_index]),
          write_batch.get());
    }

    // There might be an updated user interaction time for the group. Hence
    // write the group to DB.
    auto group_data = SavedTabGroupToData(*group);
    write_batch->WriteData(group_data.specifics().guid(),
                           group_data.SerializeAsString());
  } else {
    UpsertEntitySpecific(SavedTabGroupToData(*group), write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupTabsReorderedLocally(
    const base::Uuid& group_guid) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  DCHECK(group);

  for (const SavedTabGroupTab& tab : group->saved_tabs()) {
    UpsertEntitySpecific(SavedTabGroupTabToData(tab), write_batch.get());
  }

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

void SavedTabGroupSyncBridge::SavedTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);

  auto data = SavedTabGroupToData(*group);
  write_batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

void SavedTabGroupSyncBridge::SavedTabGroupLastUserInteractionTimeUpdated(
    const base::Uuid& group_guid) {
  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  proto::SavedTabGroupData data = SavedTabGroupToData(*group);
  write_batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::SavedTabGroupReorderedLocally() {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    UpsertEntitySpecific(SavedTabGroupToData(*group), write_batch.get());
  }

  store_->CommitWriteBatch(std::move(write_batch), base::DoNothing());
}

std::optional<std::string> SavedTabGroupSyncBridge::GetLocalCacheGuid() const {
  if (!change_processor()->IsTrackingMetadata()) {
    return std::nullopt;
  }
  return change_processor()->TrackedCacheGuid();
}

std::optional<std::string> SavedTabGroupSyncBridge::GetTrackedAccountId()
    const {
  if (!change_processor()->IsTrackingMetadata()) {
    return std::nullopt;
  }
  return change_processor()->TrackedAccountId();
}

bool SavedTabGroupSyncBridge::IsSyncing() const {
  return change_processor()->IsTrackingMetadata();
}

// static
SavedTabGroup SavedTabGroupSyncBridge::SpecificsToSavedTabGroupForTest(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  proto::SavedTabGroupData data;
  data.set_allocated_specifics(new sync_pb::SavedTabGroupSpecifics(specifics));
  return DataToSavedTabGroup(data);
}

// static
sync_pb::SavedTabGroupSpecifics
SavedTabGroupSyncBridge::SavedTabGroupToSpecificsForTest(
    const SavedTabGroup& group) {
  return SavedTabGroupToData(group).specifics();
}

// static
SavedTabGroupTab SavedTabGroupSyncBridge::SpecificsToSavedTabGroupTabForTest(
    const sync_pb::SavedTabGroupSpecifics& specifics) {
  proto::SavedTabGroupData data;
  data.set_allocated_specifics(new sync_pb::SavedTabGroupSpecifics(specifics));
  return DataToSavedTabGroupTab(data);
}

// static
sync_pb::SavedTabGroupSpecifics
SavedTabGroupSyncBridge::SavedTabGroupTabToSpecificsForTest(
    const SavedTabGroupTab& tab) {
  return SavedTabGroupTabToData(tab).specifics();
}

// static
SavedTabGroup SavedTabGroupSyncBridge::DataToSavedTabGroupForTest(
    const proto::SavedTabGroupData& data) {
  return DataToSavedTabGroup(data);
}

// static
proto::SavedTabGroupData SavedTabGroupSyncBridge::SavedTabGroupToDataForTest(
    const SavedTabGroup& group) {
  return SavedTabGroupToData(group);
}

// static
SavedTabGroupTab SavedTabGroupSyncBridge::DataToSavedTabGroupTabForTest(
    const proto::SavedTabGroupData& data) {
  return DataToSavedTabGroupTab(data);
}

// static
proto::SavedTabGroupData SavedTabGroupSyncBridge::SavedTabGroupTabToDataForTest(
    const SavedTabGroupTab& tab) {
  return SavedTabGroupTabToData(tab);
}

void SavedTabGroupSyncBridge::UpsertEntitySpecific(
    const proto::SavedTabGroupData& data,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  write_batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  SendToSync(data.specifics(), write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::RemoveEntitySpecific(
    const base::Uuid& guid,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  change_processor()->Delete(guid.AsLowercaseString(),
                             syncer::DeletionOrigin::Unspecified(),
                             write_batch->GetMetadataChangeList());
}

void SavedTabGroupSyncBridge::AddDataToLocalStorage(
    const sync_pb::SavedTabGroupSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::DataTypeStore::WriteBatch* write_batch,
    bool notify_sync) {
  base::Uuid group_guid = base::Uuid::ParseLowercase(
      specifics.has_tab() ? specifics.tab().group_guid() : specifics.guid());
  const SavedTabGroup* existing_group = model_wrapper_->GetGroup(group_guid);

  proto::SavedTabGroupData data;
  data.set_allocated_specifics(new sync_pb::SavedTabGroupSpecifics(specifics));
  std::string guid = data.specifics().guid();

  // Cases where `specifics` is a group.
  if (specifics.has_group()) {
    if (existing_group) {
      // Resolve the conflict by merging the sync and local data. Once
      // finished, write the result to the store and update sync with the new
      // merged result if appropriate.
      existing_group = model_wrapper_->MergeRemoteGroupMetadata(
          group_guid, base::UTF8ToUTF16(specifics.group().title()),
          SyncColorToTabGroupColor(specifics.group().color()),
          GroupPositionFromSpecifics(specifics),
          GetCreatorCacheGuidFromSpecifics(specifics),
          GetLastUpdaterCacheGuidFromSpecifics(specifics),
          TimeFromWindowsEpochMicros(
              specifics.update_time_windows_epoch_micros()));
      proto::SavedTabGroupData updated_data =
          SavedTabGroupToData(*existing_group);

      // Write result to the store.
      write_batch->WriteData(guid, updated_data.SerializeAsString());

      // Update sync with the new merged result.
      if (notify_sync) {
        SendToSync(std::move(updated_data.specifics()), metadata_change_list);
      }
    } else {
      // We do not have this group. Add the group from sync into local storage.
      SavedTabGroup new_group = DataToSavedTabGroup(data);
      write_batch->WriteData(guid, data.SerializeAsString());
      model_wrapper_->AddGroup(std::move(new_group));
    }

    return;
  }

  // Cases where `specifics` is a tab.
  if (specifics.has_tab()) {
    base::Uuid tab_guid = base::Uuid::ParseLowercase(data.specifics().guid());
    if (existing_group && existing_group->ContainsTab(tab_guid)) {
      // Resolve the conflict by merging the sync and local data. Once finished,
      // write the result to the store and update sync with the new merged
      // result if appropriate.
      const SavedTabGroupTab* merged_tab =
          model_wrapper_->MergeRemoteTab(DataToSavedTabGroupTab(data));
      data = SavedTabGroupTabToData(*merged_tab);

      // Write result to the store.
      write_batch->WriteData(guid, data.SerializeAsString());

      // Update sync with the new merged result.
      if (notify_sync) {
        SendToSync(std::move(data.specifics()), metadata_change_list);
      }

      return;
    }

    // We write tabs to local storage regardless of the existence of its group
    // in order to recover the tabs in the event the group was not received and
    // a crash / restart occurred.
    write_batch->WriteData(guid, data.SerializeAsString());

    if (existing_group) {
      // We do not have this tab. Add the tab from sync into local storage.
      model_wrapper_->AddTabToGroup(existing_group->saved_guid(),
                                    DataToSavedTabGroupTab(data));
    } else {
      // We reach this case if we were unable to find a group for this tab. This
      // can happen when sync sends the tab data before the group data. In this
      // case, we will store the tabs in case the group comes in later.
      tabs_missing_groups_.emplace_back(std::move(data));
    }
  }
}

void SavedTabGroupSyncBridge::DeleteDataFromLocalStorage(
    const base::Uuid& guid,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(guid.AsLowercaseString());
  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  // TODO(b/336586617): Close tabs on desktop on receiving this event.
  if (model_wrapper_->GetGroup(guid)) {
    model_wrapper_->RemoveGroup(guid);
    return;
  }

  const SavedTabGroup* group_containing_tab =
      model_wrapper_->GetGroupContainingTab(guid);
  if (group_containing_tab) {
    model_wrapper_->RemoveTabFromGroup(group_containing_tab->saved_guid(),
                                       guid);
  }
}

void SavedTabGroupSyncBridge::ResolveTabsMissingGroups(
    syncer::DataTypeStore::WriteBatch* write_batch) {
  auto tab_iterator = tabs_missing_groups_.begin();
  while (tab_iterator != tabs_missing_groups_.end()) {
    const auto& specifics = tab_iterator->specifics();
    const SavedTabGroup* group = model_wrapper_->GetGroup(
        base::Uuid::ParseLowercase(specifics.tab().group_guid()));
    if (!group) {
      base::Time last_update_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(specifics.update_time_windows_epoch_micros()));
      base::Time now = base::Time::Now();

      // Discard the tabs that do not have an associated group and have not been
      // updated within the discard threshold.
      if (now - last_update_time >= kOrphanedObjectDiscardThreshold) {
        RemoveEntitySpecific(base::Uuid::ParseLowercase(specifics.guid()),
                             write_batch);
        tab_iterator = tabs_missing_groups_.erase(tab_iterator);
      } else {
        ++tab_iterator;
      }
    } else {
      write_batch->WriteData(specifics.guid(),
                             tab_iterator->SerializeAsString());
      model_wrapper_->AddTabToGroup(group->saved_guid(),
                                    DataToSavedTabGroupTab(*tab_iterator));
      tab_iterator = tabs_missing_groups_.erase(tab_iterator);
    }
  }
}

void SavedTabGroupSyncBridge::ResolveGroupsMissingTabs(
    syncer::DataTypeStore::WriteBatch* write_batch) {
  std::vector<base::Uuid> orphaned_groups_to_destroy;
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    if (!group->saved_tabs().empty()) {
      continue;
    }

    if ((base::Time::Now() - group->update_time_windows_epoch_micros()) <
        kOrphanedObjectDiscardThreshold) {
      continue;
    }

    orphaned_groups_to_destroy.push_back(group->saved_guid());
  }

  for (const base::Uuid& group_id : orphaned_groups_to_destroy) {
    model_wrapper_->RemoveGroup(group_id);
    write_batch->DeleteData(group_id.AsLowercaseString());
  }
}

void SavedTabGroupSyncBridge::AddEntryToBatch(syncer::MutableDataBatch* batch,
                                              proto::SavedTabGroupData data) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(data.specifics()));

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;

  batch->Put(name, std::move(entity_data));
}

void SavedTabGroupSyncBridge::SendToSync(
    sync_pb::SavedTabGroupSpecifics specific,
    syncer::MetadataChangeList* metadata_change_list) {
  DCHECK(metadata_change_list);
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  auto entity_data = CreateEntityData(std::move(specific));

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;
  change_processor()->Put(name, std::move(entity_data), metadata_change_list);
}

void SavedTabGroupSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    stats::RecordMigrationResult(stats::MigrationResult::kStoreCreateFailed);
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  stats::RecordMigrationResult(stats::MigrationResult::kStoreLoadStarted);
  store_->ReadAllData(base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseLoad,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::OnDatabaseLoad(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  // This function does a series of migrations and finally loads the metadata.
  // After each migration step, the DB is read again which invokes this callback
  // again. If a migration isn't required, it will be skipped to execute the
  // next steps.
  if (error) {
    stats::RecordMigrationResult(stats::MigrationResult::kStoreLoadFailed);
    change_processor()->ReportError(*error);
    return;
  }

  // 1. Check if we haven't yet migrated from SavedTabGroupSpecifics
  // to SavedTabGroupData.
  if (!pref_service_->GetBoolean(
          prefs::kSavedTabGroupSpecificsToDataMigration)) {
    stats::RecordMigrationResult(
        stats::MigrationResult::kSpecificsToDataMigrationStarted);
    MigrateSpecificsToSavedTabGroupData(std::move(entries));
    return;
  }

  if (!migration_already_complete_recorded_) {
    migration_already_complete_recorded_ = true;
    stats::RecordMigrationResult(
        stats::MigrationResult::kSpecificsToDataMigrationAlreadyComplete);
  }

  // 2. If we are done with all the migrations, proceed with regular metadata
  // loading.
  stats::RecordMigrationResult(stats::MigrationResult::kStoreLoadCompleted);
  store_->ReadAllMetadata(
      base::BindOnce(&SavedTabGroupSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entries)));
}

void SavedTabGroupSyncBridge::MigrateSpecificsToSavedTabGroupData(
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  int parse_failure_count = 0;
  for (const syncer::DataTypeStore::Record& r : *entries) {
    sync_pb::SavedTabGroupSpecifics specifics;
    // We might potentially be parsing a SavedTabGroupData as a
    // SavedTabGroupSpecifics and vice versa. At times parsing succeeds, hence
    // we need to check for existence of required fields.
    if (!specifics.ParseFromString(r.value) || !IsValidSpecifics(specifics)) {
      DLOG(WARNING)
          << "Failed to parse SavedTabGroupSpecifics during migration";
      parse_failure_count++;
      continue;
    }

    proto::SavedTabGroupData new_data;
    new_data.set_allocated_specifics(
        new sync_pb::SavedTabGroupSpecifics(specifics));

    batch->WriteData(specifics.guid(), new_data.SerializeAsString());
  }

  if (parse_failure_count > 0) {
    stats::RecordMigrationResult(
        stats::MigrationResult::
            kSpecificsToDataMigrationParseFailedAtLeastOnce);
    stats::RecordSpecificsParseFailureCount(parse_failure_count,
                                            entries->size());
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &SavedTabGroupSyncBridge::OnSpecificsToDataMigrationComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::OnSpecificsToDataMigrationComplete(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    stats::RecordMigrationResult(
        stats::MigrationResult::kSpecificsToDataMigrationWriteFailed);
    change_processor()->ReportError(*error);
    DLOG(WARNING) << "Failed to write migrated data";
    return;
  }

  // Migration successful, write it to prefs, and read the DB again.
  pref_service_->SetBoolean(prefs::kSavedTabGroupSpecificsToDataMigration,
                            true);
  stats::RecordMigrationResult(
      stats::MigrationResult::kSpecificsToDataMigrationSuccess);
  store_->ReadAllData(base::BindOnce(&SavedTabGroupSyncBridge::OnDatabaseLoad,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedTabGroupSyncBridge::OnReadAllMetadata(
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "SavedTabGroupSyncBridge::OnReadAllMetadata");
  if (error) {
    stats::RecordMigrationResult(
        stats::MigrationResult::kReadAllMetadataFailed);
    change_processor()->ReportError({FROM_HERE, "Failed to read metadata."});
    return;
  }

  stats::RecordMigrationResult(stats::MigrationResult::kReadAllMetadataSuccess);

  std::vector<proto::SavedTabGroupData> stored_entries;
  stored_entries.reserve(entries->size());

  for (const syncer::DataTypeStore::Record& r : *entries) {
    proto::SavedTabGroupData proto;
    if (!proto.ParseFromString(r.value)) {
      continue;
    }
    stored_entries.emplace_back(std::move(proto));
  }

  stats::RecordParsedSavedTabGroupDataCount(stored_entries.size(),
                                            entries->size());

  // Update `model_` with any data stored in local storage except for orphaned
  // tabs. Orphaned tabs will be returned and added to `tabs_missing_groups_` in
  // case their missing group ever arrives.
  tabs_missing_groups_ =
      LoadStoredEntries(std::move(stored_entries), model_wrapper_);

  // change_process() will ignore the following call in case of error.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SavedTabGroupSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to save metadata."});
    return;
  }

  // TODO(dljames): React to store failures when a save is not successful.
}

void SavedTabGroupSyncBridge::UpdateLocalCacheGuidForGroups(
    syncer::DataTypeStore::WriteBatch* write_batch) {
  std::pair<std::set<base::Uuid>, std::set<base::Uuid>> updated_ids =
      model_wrapper_->UpdateLocalCacheGuid(std::nullopt, GetLocalCacheGuid());
  const std::set<base::Uuid>& updated_group_ids = updated_ids.first;
  const std::set<base::Uuid>& updated_tab_ids = updated_ids.second;

  for (const base::Uuid& saved_guid : updated_group_ids) {
    proto::SavedTabGroupData data =
        SavedTabGroupToData(*model_wrapper_->GetGroup(saved_guid));
    write_batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  }

  for (const base::Uuid& tab_guid : updated_tab_ids) {
    const SavedTabGroup* group =
        model_wrapper_->GetGroupContainingTab(tab_guid);
    CHECK(group);
    const SavedTabGroupTab* tab = group->GetTab(tab_guid);
    CHECK(tab);
    proto::SavedTabGroupData data = SavedTabGroupTabToData(*tab);
    write_batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  }
}

bool SavedTabGroupSyncBridge::IsRemoteGroup(const SavedTabGroup& group) {
  std::optional<std::string> local_cache_guid = GetLocalCacheGuid();
  std::optional<std::string> group_cache_guid = group.creator_cache_guid();
  if (!local_cache_guid || !group_cache_guid) {
    return false;
  }

  return local_cache_guid.value() != group_cache_guid.value();
}

}  // namespace tab_groups
