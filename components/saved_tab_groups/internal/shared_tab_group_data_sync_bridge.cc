// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"

#include <map>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/proto/shared_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

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
  CHECK(group.is_shared_tab_group());
  sync_pb::SharedTabGroupDataSpecifics pb_specifics;
  pb_specifics.set_guid(group.saved_guid().AsLowercaseString());
  pb_specifics.set_update_time_windows_epoch_micros(
      group.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SharedTabGroup* pb_group = pb_specifics.mutable_tab_group();
  pb_group->set_color(TabGroupColorToSyncColor(group.color()));
  pb_group->set_title(base::UTF16ToUTF8(group.title()));
  if (group.originating_saved_tab_group_guid().has_value()) {
    pb_group->set_originating_tab_group_guid(
        group.originating_saved_tab_group_guid().value().AsLowercaseString());
  }
  return pb_specifics;
}

SavedTabGroup SpecificsToSharedTabGroup(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  CHECK(specifics.has_tab_group());
  CHECK(!collaboration_id.empty());

  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specifics.tab_group().color());
  std::u16string title = base::UTF8ToUTF16(specifics.tab_group().title());
  base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  base::Uuid originating_saved_tab_group_guid;
  if (specifics.tab_group().has_originating_tab_group_guid()) {
    originating_saved_tab_group_guid = base::Uuid::ParseLowercase(
        specifics.tab_group().originating_tab_group_guid());
  }

  // GUID must be checked before this method is called.
  CHECK(guid.is_valid());

  const base::Time update_time =
      TimeFromWindowsEpochMicros(specifics.update_time_windows_epoch_micros());

  SavedTabGroup group(title, color, /*urls=*/{}, /*position=*/std::nullopt,
                      std::move(guid), /*local_group_id=*/std::nullopt,
                      /*creator_cache_guid=*/std::nullopt,
                      /*last_updater_cache_guid=*/std::nullopt,
                      /*created_before_syncing_tab_groups=*/false,
                      /*creation_time_windows_epoch_micros=*/std::nullopt,
                      update_time);
  group.SetCollaborationId(collaboration_id);
  if (originating_saved_tab_group_guid.is_valid()) {
    group.SetOriginatingSavedTabGroupGuid(
        std::move(originating_saved_tab_group_guid));
  }
  return group;
}

SavedTabGroupTab SpecificsToSharedTabGroupTab(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    size_t position) {
  CHECK(specifics.has_tab());

  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());

  // GUID must be checked before this method is called.
  CHECK(guid.is_valid());

  const base::Time update_time =
      TimeFromWindowsEpochMicros(specifics.update_time_windows_epoch_micros());

  SavedTabGroupTab tab(
      GURL(specifics.tab().url()), base::UTF8ToUTF16(specifics.tab().title()),
      base::Uuid::ParseLowercase(specifics.tab().shared_tab_group_guid()),
      position, guid);
  tab.SetUpdateTimeWindowsEpochMicros(update_time);
  return tab;
}

sync_pb::SharedTabGroupDataSpecifics SharedTabGroupTabToSpecifics(
    const SavedTabGroupTab& tab,
    sync_pb::UniquePosition unique_position) {
  sync_pb::SharedTabGroupDataSpecifics specifics;

  specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  specifics.set_update_time_windows_epoch_micros(
      tab.update_time_windows_epoch_micros()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());

  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_url(tab.url().spec());
  pb_tab->set_shared_tab_group_guid(tab.saved_group_guid().AsLowercaseString());
  pb_tab->set_title(base::UTF16ToUTF8(tab.title()));
  *pb_tab->mutable_unique_position() = std::move(unique_position);
  return specifics;
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    sync_pb::SharedTabGroupDataSpecifics specifics,
    const std::string& collaboration_id) {
  CHECK(!collaboration_id.empty());
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specifics.guid();
  entity_data->specifics.mutable_shared_tab_group_data()->Swap(&specifics);
  entity_data->collaboration_id = collaboration_id;
  return entity_data;
}

void AddEntryToBatch(syncer::MutableDataBatch* batch,
                     sync_pb::SharedTabGroupDataSpecifics specifics,
                     const std::string& collaboration_id) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(specifics), collaboration_id);

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;

  batch->Put(name, std::move(entity_data));
}

std::string ExtractCollaborationId(
    const syncer::EntityMetadataMap& sync_metadata,
    const std::string& storage_key) {
  auto it = sync_metadata.find(storage_key);
  if (it == sync_metadata.end()) {
    return std::string();
  }

  return it->second->collaboration().collaboration_id();
}

// Parses stored entries and populates the result to the `on_load_callback`.
std::vector<sync_pb::SharedTabGroupDataSpecifics> LoadStoredEntries(
    const std::vector<proto::SharedTabGroupData>& stored_entries,
    SyncBridgeTabGroupModelWrapper* model_wrapper,
    const syncer::EntityMetadataMap& sync_metadata) {
  DVLOG(2) << "Loading SharedTabGroupData entries from the disk: "
           << stored_entries.size();

  std::vector<SavedTabGroup> groups;
  std::unordered_set<std::string> group_guids;

  // `stored_entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (const proto::SharedTabGroupData& proto : stored_entries) {
    const sync_pb::SharedTabGroupDataSpecifics& specifics = proto.specifics();
    if (!specifics.has_tab_group()) {
      continue;
    }
    // Collaboration ID is stored as part of sync metadata.
    const std::string& storage_key = specifics.guid();
    std::string collaboration_id =
        ExtractCollaborationId(sync_metadata, storage_key);
    if (collaboration_id.empty()) {
      DVLOG(2) << "Entry is missing collaboration ID: " << storage_key;
      continue;
    }
    groups.emplace_back(SpecificsToSharedTabGroup(specifics, collaboration_id));
    // Load remaining local-only fields.
    if (AreLocalIdsPersisted() &&
        proto.local_group_data().has_local_group_id()) {
      groups.back().SetLocalGroupId(
          LocalTabGroupIDFromString(proto.local_group_data().local_group_id()));
    }
    // TODO(crbug.com/364910060): resolve duplicate GUIDs.
    group_guids.emplace(specifics.guid());
  }

  // Parse tabs and find tabs missing groups.
  std::vector<sync_pb::SharedTabGroupDataSpecifics> tabs_missing_groups;
  std::vector<SavedTabGroupTab> tabs;
  for (const proto::SharedTabGroupData& proto : stored_entries) {
    const sync_pb::SharedTabGroupDataSpecifics& specifics = proto.specifics();
    if (!specifics.has_tab()) {
      continue;
    }
    const std::string& storage_key = specifics.guid();
    if (ExtractCollaborationId(sync_metadata, storage_key).empty()) {
      // Collaboration ID is not strictly required (tabs rely on parent group's
      // collaboration IDs) but check it here for consistency anyway.
      DVLOG(2) << "Entry is missing collaboration ID: " << storage_key;
    }
    if (group_guids.contains(specifics.tab().shared_tab_group_guid())) {
      // TODO(crbug.com/351357559): calculate the position based on unique
      // positions from metadata.
      tabs.emplace_back(SpecificsToSharedTabGroupTab(specifics, 0));
      continue;
    }
    tabs_missing_groups.push_back(specifics);
  }

  model_wrapper->Initialize(std::move(groups), std::move(tabs));
  return tabs_missing_groups;
}

void StoreSharedTab(syncer::DataTypeStore::WriteBatch* write_batch,
                    sync_pb::SharedTabGroupDataSpecifics specifics) {
  std::string storage_key = specifics.guid();
  proto::SharedTabGroupData local_proto;
  local_proto.mutable_specifics()->Swap(&specifics);
  write_batch->WriteData(storage_key, local_proto.SerializeAsString());
}

void StoreSharedGroup(syncer::DataTypeStore::WriteBatch* write_batch,
                      sync_pb::SharedTabGroupDataSpecifics specifics,
                      proto::LocalSharedTabGroupData local_group_data) {
  std::string storage_key = specifics.guid();
  proto::SharedTabGroupData local_proto;
  *local_proto.mutable_specifics() = std::move(specifics);
  *local_proto.mutable_local_group_data() = std::move(local_group_data);
  write_batch->WriteData(storage_key, local_proto.SerializeAsString());
}

proto::LocalSharedTabGroupData GroupToLocalOnlyData(
    const SavedTabGroup& group) {
  proto::LocalSharedTabGroupData local_group_data;
  if (AreLocalIdsPersisted() && group.local_group_id().has_value()) {
    local_group_data.set_local_group_id(
        LocalTabGroupIDToString(group.local_group_id().value()));
  }
  return local_group_data;
}

std::string StorageKeyForTab(const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString();
}

syncer::ClientTagHash ClientTagHashForTab(const SavedTabGroupTab& tab) {
  return syncer::ClientTagHash::FromUnhashed(
      syncer::SHARED_TAB_GROUP_DATA, /*client_tag=*/StorageKeyForTab(tab));
}

std::string StorageKeyForTabInGroup(const SavedTabGroup& group,
                                    size_t tab_index) {
  CHECK_LT(tab_index, group.saved_tabs().size());
  return StorageKeyForTab(group.saved_tabs()[tab_index]);
}

// Returns the preferred index for the existing tab. The adjustment is required
// in case the tab is moved to a larger index because tab positions get shifted
// be one.
// For example, if the tab is moved from a position 1 (`current_index`) before
// another tab at index 5 (`position_insert_before`), the new position for the
// tab being moved is 4.
size_t AdjustPreferredTabIndex(size_t position_insert_before,
                               size_t current_index) {
  if (position_insert_before > current_index) {
    return position_insert_before - 1;
  }
  return position_insert_before;
}

// Compares two elements in the reversed order based on their unique positions.
// Entities with invalid positions are considered greater than entities with
// valid positions.
bool ReversedUniquePositionComparison(
    const std::unique_ptr<syncer::EntityChange>& left,
    const std::unique_ptr<syncer::EntityChange>& right) {
  syncer::UniquePosition left_unique_position =
      syncer::UniquePosition::FromProto(left->data()
                                            .specifics.shared_tab_group_data()
                                            .tab()
                                            .unique_position());
  if (!left_unique_position.IsValid()) {
    // `left` (invalid) == `right` (invalid).
    // `left` (invalid) > `right` (valid).
    return false;
  }
  syncer::UniquePosition right_unique_position =
      syncer::UniquePosition::FromProto(right->data()
                                            .specifics.shared_tab_group_data()
                                            .tab()
                                            .unique_position());
  if (!right_unique_position.IsValid()) {
    // `left` (valid) < `right` (invalid).
    return true;
  }
  return right_unique_position.LessThan(left_unique_position);
}

// Sorts the tab changes (only additions and updates, without deletions) in the
// reversed order by their unique positions. If some updates do not have a valid
// unique position, they are placed to the end in an unspecified order.
void SortByUniquePositionFromRightToLeft(
    std::vector<std::unique_ptr<syncer::EntityChange>>& tab_changes) {
  base::ranges::sort(tab_changes, &ReversedUniquePositionComparison);
}

}  // namespace

SharedTabGroupDataSyncBridge::SharedTabGroupDataSyncBridge(
    SyncBridgeTabGroupModelWrapper* model_wrapper,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    PrefService* pref_service)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      model_wrapper_(model_wrapper) {
  CHECK(model_wrapper_);

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

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
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
              change->data().collaboration_id, metadata_change_list.get(),
              write_batch.get());
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
  // TODO(crbug.com/351357559): fix the order of applying updates (groups after
  // tabs).
  for (const std::string& entity : deleted_entities) {
    DeleteDataFromLocalStorage(entity, write_batch.get());
  }

  // Sort tab updates and creations in the reversed order. This is required to
  // apply updates from right to left within one group to avoid unnecessary
  // reordering during applying updates. See a corresponding test case
  // ShouldKeepTabsOrderDuringRemoteUpdate for example. Note that ordering by
  // groups is not required (although could be done for optimization). Tab
  // updates with invalid unique positions are applied last.
  SortByUniquePositionFromRightToLeft(tab_updates);

  std::set<base::Uuid> tab_ids_with_pending_model_update;
  for (const std::unique_ptr<syncer::EntityChange>& change : tab_updates) {
    // TODO(crbug.com/351357559): consider duplicate GUIDs with different
    // collaboration IDs.
    tab_ids_with_pending_model_update.insert(base::Uuid::ParseLowercase(
        change->data().specifics.shared_tab_group_data().guid()));
  }

  // Process tab updates after applying deletions so that tab updates having
  // deleted groups will be stored to `tabs_missing_groups_`.
  for (const std::unique_ptr<syncer::EntityChange>& change : tab_updates) {
    ApplyRemoteTabUpdate(change->data().specifics.shared_tab_group_data(),
                         metadata_change_list.get(), write_batch.get(),
                         tab_ids_with_pending_model_update);

    // The tab update has been applied to the model.
    tab_ids_with_pending_model_update.erase(base::Uuid::ParseLowercase(
        change->data().specifics.shared_tab_group_data().guid()));
  }

  // TODO(crbug.com/370719750): resolve and handle tabs missing groups later.
  // ResolveTabsMissingGroups(write_batch.get());

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupDataSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  std::set<base::Uuid> parsed_guids;
  for (const std::string& guid : storage_keys) {
    base::Uuid parsed_guid = base::Uuid::ParseLowercase(guid);
    CHECK(parsed_guid.is_valid());
    parsed_guids.insert(std::move(parsed_guid));
  }

  // Iterate over all the shared groups and tabs to find corresponding entities
  // for commit.
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    CHECK(group->collaboration_id().has_value());

    if (parsed_guids.contains(group->saved_guid())) {
      AddEntryToBatch(batch.get(), SharedTabGroupToSpecifics(*group),
                      group->collaboration_id().value());
    }
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      if (parsed_guids.contains(tab.saved_tab_guid())) {
        AddEntryToBatch(
            batch.get(),
            SharedTabGroupTabToSpecifics(
                tab, change_processor()->GetUniquePositionForStorageKey(
                         StorageKeyForTab(tab))),
            group->collaboration_id().value());
      }
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    CHECK(group->collaboration_id().has_value());
    AddEntryToBatch(batch.get(), SharedTabGroupToSpecifics(*group),
                    group->collaboration_id().value());
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      AddEntryToBatch(
          batch.get(),
          SharedTabGroupTabToSpecifics(
              tab, change_processor()->GetUniquePositionForStorageKey(
                       StorageKeyForTab(tab))),
          group->collaboration_id().value());
    }
  }
  return batch;
}

std::string SharedTabGroupDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.shared_tab_group_data().guid() + "|" +
         entity_data.collaboration_id;
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

bool SharedTabGroupDataSyncBridge::SupportsUniquePositions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

sync_pb::UniquePosition SharedTabGroupDataSyncBridge::GetUniquePosition(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only tabs support unique positions.
  if (specifics.shared_tab_group_data().has_tab()) {
    return specifics.shared_tab_group_data().tab().unique_position();
  }
  return sync_pb::UniquePosition();
}

void SharedTabGroupDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // When the sync is disabled, all the corresponding groups and their tabs
  // should be closed. To do that, each of the tab needs to be closed
  // explicitly, otherwise they would remain open.

  // First, collect the GUIDs for all the shared tab groups and their tabs. This
  // is required to delete them from the model in a separate loop, otherwise
  // removing them from within the same loop would modify the same underlying
  // storage.
  std::map<base::Uuid, std::vector<base::Uuid>> group_and_tabs_to_close_locally;
  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    std::vector<base::Uuid> tabs_to_close_locally;
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      tabs_to_close_locally.emplace_back(tab.saved_tab_guid());
    }

    // Normally, groups don't need to be closed explicitly because closing the
    // last tab closes a corresponding group. However if a group is empty, it
    // would left open. It's safer to explicitly close all the groups explicitly
    // (the model will just ignore it if they don't exist anymore), hence keep
    // an empty group as well.
    group_and_tabs_to_close_locally[group->saved_guid()] =
        std::move(tabs_to_close_locally);
  }

  for (const auto& [group_id, tabs_to_close_locally] :
       group_and_tabs_to_close_locally) {
    for (const base::Uuid& tab_id : tabs_to_close_locally) {
      model_wrapper_->RemoveTabFromGroup(group_id, tab_id);
    }

    // The group could be removed when the last tab is closed.
    if (model_wrapper_->GetGroup(group_id)) {
      model_wrapper_->RemoveGroup(group_id);
    }
  }

  // Delete all shared tabs and sync metadata from the store.
  // `delete_metadata_change_list` is not used because all the metadata is
  // deleted anyway.
  store_->DeleteAllDataAndMetadata(base::DoNothing());
}

sync_pb::EntitySpecifics
SharedTabGroupDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return DataTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
      entity_specifics);
}

bool SharedTabGroupDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (entity_data.collaboration_id.empty()) {
    DVLOG(2) << "Remote Shared Tab Group is missing collaboration ID";
    return false;
  }
  const sync_pb::SharedTabGroupDataSpecifics& specifics =
      entity_data.specifics.shared_tab_group_data();
  if (!base::Uuid::ParseLowercase(specifics.guid()).is_valid()) {
    return false;
  }
  if (!specifics.has_tab_group() && !specifics.has_tab()) {
    return false;
  }
  return true;
}

void SharedTabGroupDataSyncBridge::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !model_wrapper_->IsInitialized()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupAddedLocally called while not initialized";
    return;
  }

  const SavedTabGroup* group = model_wrapper_->GetGroup(guid);
  CHECK(group);
  CHECK(group->is_shared_tab_group());

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  CHECK(group->collaboration_id().has_value());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      SharedTabGroupToSpecifics(*group);
  StoreSharedGroup(write_batch.get(), group_specifics,
                   GroupToLocalOnlyData(*group));
  SendToSync(group_specifics, group->collaboration_id().value(),
             write_batch->GetMetadataChangeList());
  for (size_t i = 0; i < group->saved_tabs().size(); ++i) {
    const SavedTabGroupTab& tab = group->saved_tabs()[i];
    sync_pb::UniquePosition unique_position =
        (i == 0) ? change_processor()->UniquePositionForInitialEntity(
                       ClientTagHashForTab(tab))
                 : change_processor()->UniquePositionAfter(
                       StorageKeyForTab(group->saved_tabs()[i - 1]),
                       ClientTagHashForTab(tab));
    sync_pb::SharedTabGroupDataSpecifics tab_specifics =
        SharedTabGroupTabToSpecifics(tab, std::move(unique_position));
    StoreSharedGroup(write_batch.get(), tab_specifics,
                     GroupToLocalOnlyData(*group));
    SendToSync(tab_specifics, group->collaboration_id().value(),
               write_batch->GetMetadataChangeList());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !model_wrapper_->IsInitialized()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupUpdatedLocally called while not initialized";
    return;
  }

  // The bridge must be called for the shared tab groups only which is
  // guaranteed by the TabGroupSyncBridgeMediator.
  const SavedTabGroup* group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);
  CHECK(group->is_shared_tab_group());

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  if (tab_guid.has_value()) {
    // The tab has been updated, added or removed.
    ProcessTabLocalChange(*group, tab_guid.value(), write_batch.get());
  } else {
    // Only group metadata has been updated.
    sync_pb::SharedTabGroupDataSpecifics specifics =
        SharedTabGroupToSpecifics(*group);
    StoreSharedGroup(write_batch.get(), specifics,
                     GroupToLocalOnlyData(*group));
    SendToSync(specifics, group->collaboration_id().value(),
               write_batch->GetMetadataChangeList());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !model_wrapper_->IsInitialized()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupRemovedLocally called while not initialized";
    return;
  }

  CHECK(removed_group.is_shared_tab_group());

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // Intentionally only remove the group (creating orphaned tabs in the
  // process), so other devices with the group open in the Tabstrip can react to
  // the deletion appropriately (i.e. We do not have to determine if a tab
  // deletion was part of a group deletion).
  // TODO(crbug.com/319521964): consider if this is required for shared tab
  // groups.
  RemoveEntitySpecifics(removed_group.saved_guid(), write_batch.get());

  // TODO(crbug.com/370719750): handle tabs missing groups.
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::ProcessTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !model_wrapper_->IsInitialized()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupLocalIdChanged called while not initialized";
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);

  StoreSharedGroup(write_batch.get(), SharedTabGroupToSpecifics(*group),
                   GroupToLocalOnlyData(*group));

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
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
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  std::vector<proto::SharedTabGroupData> stored_entries;
  stored_entries.reserve(entries->size());

  for (const syncer::DataTypeStore::Record& r : *entries) {
    proto::SharedTabGroupData proto;
    if (!proto.ParseFromString(r.value)) {
      continue;
    }
    stored_entries.emplace_back(std::move(proto));
  }

  // TODO(crbug.com/370719750): Handle tabs missing groups.
  LoadStoredEntries(std::move(stored_entries), model_wrapper_,
                    metadata_batch->GetAllMetadata());
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SharedTabGroupDataSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to store data."});
  }
}

void SharedTabGroupDataSyncBridge::AddGroupToLocalStorage(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  base::Uuid group_guid = base::Uuid::ParseLowercase(specifics.guid());
  if (!group_guid.is_valid()) {
    // Ignore remote updates having invalid data.
    return;
  }

  CHECK(specifics.has_tab_group());

  if (!model_wrapper_->GetGroup(group_guid)) {
    // This is a new remotely created group. Add the group from sync into local
    // storage. Note that on some platforms new remote groups may open in the
    // tab strip, and associate its local group ID. This is currently prevented
    // by delaying observer calls in the TabGroupSyncService.
    StoreSharedGroup(write_batch, specifics, proto::LocalSharedTabGroupData());
    model_wrapper_->AddGroup(
        SpecificsToSharedTabGroup(specifics, collaboration_id));
    return;
  }

  // Update the existing group with remote data.
  const SavedTabGroup* existing_group =
      model_wrapper_->MergeRemoteGroupMetadata(
          group_guid, base::UTF8ToUTF16(specifics.tab_group().title()),
          SyncColorToTabGroupColor(specifics.tab_group().color()),
          /*position=*/std::nullopt,
          /*creator_cache_guid=*/std::nullopt,
          /*last_updater_cache_guid=*/std::nullopt,
          TimeFromWindowsEpochMicros(
              specifics.update_time_windows_epoch_micros()));
  CHECK(existing_group);

  // TODO(crbug.com/319521964): consider checking that collaboration ID never
  // changes.

  // Create new specifics in case some fields were merged.
  sync_pb::SharedTabGroupDataSpecifics updated_specifics =
      SharedTabGroupToSpecifics(*existing_group);

  StoreSharedGroup(write_batch, updated_specifics,
                   GroupToLocalOnlyData(*existing_group));
}

void SharedTabGroupDataSyncBridge::ApplyRemoteTabUpdate(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::DataTypeStore::WriteBatch* write_batch,
    const std::set<base::Uuid>& tab_ids_with_pending_model_update) {
  CHECK(specifics.has_tab());

  base::Uuid tab_guid = base::Uuid::ParseLowercase(specifics.guid());
  CHECK(tab_guid.is_valid());
  base::Uuid group_guid =
      base::Uuid::ParseLowercase(specifics.tab().shared_tab_group_guid());
  if (!group_guid.is_valid()) {
    // Ignore tab with invalid data.
    return;
  }

  const SavedTabGroup* existing_group = model_wrapper_->GetGroup(group_guid);
  if (existing_group && existing_group->ContainsTab(tab_guid)) {
    const std::optional<int> current_tab_index =
        existing_group->GetIndexOfTab(tab_guid);
    CHECK(current_tab_index.has_value());
    const size_t position_insert_before = PositionToInsertRemoteTab(
        specifics.tab().unique_position(), *existing_group,
        tab_ids_with_pending_model_update);

    const SavedTabGroupTab* merged_tab =
        model_wrapper_->MergeRemoteTab(SpecificsToSharedTabGroupTab(
            specifics, AdjustPreferredTabIndex(position_insert_before,
                                               current_tab_index.value())));

    // Unique positions are stored by sync in sync metadata.
    sync_pb::SharedTabGroupDataSpecifics merged_entry =
        SharedTabGroupTabToSpecifics(*merged_tab, sync_pb::UniquePosition());

    // Write result to the store.
    StoreSharedTab(write_batch, std::move(merged_entry));
    return;
  }

  // Tabs are stored to the local storage regardless of the existence of its
  // group in order to recover the tabs in the event the group was not received
  // and a crash / restart occurred.
  // TODO(crbug.com/351357559): do not store unique position outside of sync
  // metadata.
  StoreSharedTab(write_batch, specifics);

  if (existing_group) {
    // This is a new tab for the group.
    model_wrapper_->AddTabToGroup(
        existing_group->saved_guid(),
        SpecificsToSharedTabGroupTab(
            specifics, PositionToInsertRemoteTab(
                           specifics.tab().unique_position(), *existing_group,
                           tab_ids_with_pending_model_update)));
  } else {
    // The tab does not have a corresponding group. This can happen when sync
    // sends the tab data before the group data. In this case, the tab is stored
    // in case the group comes in later.
    // TODO(crbug.com/370719750): keep tabs with no groups.
  }
}

void SharedTabGroupDataSyncBridge::DeleteDataFromLocalStorage(
    const std::string& storage_key,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  write_batch->DeleteData(storage_key);

  base::Uuid guid = base::Uuid::ParseLowercase(storage_key);
  if (!guid.is_valid()) {
    return;
  }

  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  if (model_wrapper_->GetGroup(guid)) {
    model_wrapper_->RemoveGroup(guid);
    return;
  }

  if (const SavedTabGroup* group_containing_tab =
          model_wrapper_->GetGroupContainingTab(guid)) {
    model_wrapper_->RemoveTabFromGroup(group_containing_tab->saved_guid(),
                                       guid);
  }
}

void SharedTabGroupDataSyncBridge::SendToSync(
    sync_pb::SharedTabGroupDataSpecifics specific,
    const std::string& collaboration_id,
    syncer::MetadataChangeList* metadata_change_list) {
  CHECK(metadata_change_list);
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(specific), collaboration_id);

  // Copy because our key is the name of `entity_data`.
  std::string storage_key = GetStorageKey(*entity_data);
  change_processor()->Put(storage_key, std::move(entity_data),
                          metadata_change_list);
}

void SharedTabGroupDataSyncBridge::ProcessTabLocalChange(
    const SavedTabGroup& group,
    const base::Uuid& tab_id,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  CHECK(group.collaboration_id().has_value());

  std::optional<int> tab_index = group.GetIndexOfTab(tab_id);
  if (!tab_index) {
    // Process tab deletion.
    RemoveEntitySpecifics(tab_id, write_batch);
    return;
  }

  CHECK_LT(tab_index.value(), std::ssize(group.saved_tabs()));
  CHECK_GE(tab_index.value(), 0);

  // Process new or updated tab.
  // TODO(crbug.com/351357559): verify position handling in case of bulk update.
  sync_pb::SharedTabGroupDataSpecifics specifics = SharedTabGroupTabToSpecifics(
      group.saved_tabs()[tab_index.value()],
      CalculateUniquePosition(group, tab_index.value()));
  StoreSharedTab(write_batch, specifics);
  SendToSync(specifics, group.collaboration_id().value(),
             write_batch->GetMetadataChangeList());
}

void SharedTabGroupDataSyncBridge::RemoveEntitySpecifics(
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

sync_pb::UniquePosition SharedTabGroupDataSyncBridge::CalculateUniquePosition(
    const SavedTabGroup& group,
    size_t tab_index) const {
  CHECK_LT(tab_index, group.saved_tabs().size());
  syncer::ClientTagHash client_tag_hash =
      ClientTagHashForTab(group.saved_tabs()[tab_index]);

  if (group.saved_tabs().size() == 1) {
    // The tab is the only one in the group.
    return change_processor()->UniquePositionForInitialEntity(client_tag_hash);
  }

  if (tab_index == 0) {
    // The tab is the first one.
    return change_processor()->UniquePositionBefore(
        StorageKeyForTabInGroup(group, tab_index + 1), client_tag_hash);
  }

  if (tab_index == group.saved_tabs().size() - 1) {
    // The tab is the last one.
    return change_processor()->UniquePositionAfter(
        StorageKeyForTabInGroup(group, tab_index - 1), client_tag_hash);
  }

  return change_processor()->UniquePositionBetween(
      StorageKeyForTabInGroup(group, tab_index - 1),
      StorageKeyForTabInGroup(group, tab_index + 1), client_tag_hash);
}

size_t SharedTabGroupDataSyncBridge::PositionToInsertRemoteTab(
    const sync_pb::UniquePosition& remote_unique_position,
    const SavedTabGroup& group,
    const std::set<base::Uuid>& tab_ids_to_ignore) const {
  syncer::UniquePosition parsed_remote_position =
      syncer::UniquePosition::FromProto(remote_unique_position);
  if (!parsed_remote_position.IsValid()) {
    DVLOG(1) << "Invalid remote unique position";
    return group.saved_tabs().size();
  }

  // Find the first local tab index before which the new tab should be inserted.
  for (size_t i = 0; i < group.saved_tabs().size(); ++i) {
    if (tab_ids_to_ignore.contains(group.saved_tabs()[i].saved_tab_guid())) {
      // Skip tabs which will be updated later because their unique positions
      // are already updated in the processor and they can't be used until the
      // update is applied to the model (because the ordering of tabs may be now
      // inconsistent between the model and the processor).
      // This is similar to removing all the updated tabs from the model, and
      // then adding them one by one considering the right order.
      continue;
    }
    syncer::UniquePosition local_position = syncer::UniquePosition::FromProto(
        change_processor()->GetUniquePositionForStorageKey(
            StorageKeyForTabInGroup(group, i)));
    if (!local_position.IsValid()) {
      // Normally, this should not happen. In case the data is inconsistent,
      // prefer to insert a valid position in the correct order before the tab
      // with invalid unique position.
      DVLOG(1) << "Invalid local position for tab at index " << i;
      return i;
    }

    // Unique positions can be equal only in case it's the same as the local and
    // the tab's position does not really change (unique positions are based on
    // entity's client tag). In this case just return the same position.
    // `parsed_remote_position` <= `local_position`.
    if (!local_position.LessThan(parsed_remote_position)) {
      // Insert the remote tab before the current local tab or keep the existing
      // tab at the same place.
      return i;
    }
  }

  return group.saved_tabs().size();
}

}  // namespace tab_groups
