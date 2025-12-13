// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/data_sharing/public/logger.h"
#include "components/data_sharing/public/logger_utils.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/stats.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/proto/shared_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/collaboration_metadata.h"
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
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }

  NOTREACHED() << "No known conversion for the supplied color.";
}

base::Time TimeFromWindowsEpochMicros(int64_t time_windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(time_windows_epoch_micros));
}

SavedTabGroup SpecificsToSharedTabGroup(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const syncer::CollaborationMetadata& collaboration_metadata,
    base::Time creation_time,
    bool use_originating_tab_group_guid) {
  CHECK(specifics.has_tab_group());
  CHECK(!collaboration_metadata.collaboration_id()->empty());

  const tab_groups::TabGroupColorId color =
      SyncColorToTabGroupColor(specifics.tab_group().color());
  std::u16string title = base::UTF8ToUTF16(specifics.tab_group().title());
  base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  base::Uuid originating_tab_group_guid;
  if (specifics.tab_group().has_originating_tab_group_guid()) {
    originating_tab_group_guid = base::Uuid::ParseLowercase(
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
                      creation_time);
  group.SetCollaborationId(
      syncer::CollaborationId(collaboration_metadata.collaboration_id()));
  group.SetCreatedByAttribution(collaboration_metadata.created_by());
  group.SetUpdatedByAttribution(collaboration_metadata.last_updated_by());
  if (originating_tab_group_guid.is_valid()) {
    group.SetOriginatingTabGroupGuid(std::move(originating_tab_group_guid),
                                     use_originating_tab_group_guid);
  }

  // Set the remote update time explicitly because the setters above could have
  // updated it.
  group.SetUpdateTime(update_time);
  return group;
}

SavedTabGroupTab SpecificsToSharedTabGroupTab(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const syncer::CollaborationMetadata& collaboration_metadata,
    size_t position,
    base::Time creation_time,
    base::Time modification_time,
    bool sanitize_url_and_title) {
  CHECK(specifics.has_tab());

  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());

  // GUID must be checked before this method is called.
  CHECK(guid.is_valid());

  const base::Time update_time =
      TimeFromWindowsEpochMicros(specifics.update_time_windows_epoch_micros());

  GURL url = GURL(specifics.tab().url());
  std::string title = specifics.tab().title();
  if (sanitize_url_and_title && !IsURLValidForSavedTabGroups(url)) {
    url = GURL(kChromeSavedTabGroupUnsupportedURL);
    title.clear();
  }
  SavedTabGroupTab tab(
      url, base::UTF8ToUTF16(title),
      base::Uuid::ParseLowercase(specifics.tab().shared_tab_group_guid()),
      position, guid, /*local_tab_id=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt, creation_time);
  tab.SetCreatedByAttribution(collaboration_metadata.created_by());
  tab.SetUpdatedByAttribution(collaboration_metadata.last_updated_by());
  tab.SetUpdateTime(update_time);
  tab.SetNavigationTime(modification_time);
  return tab;
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    sync_pb::SharedTabGroupDataSpecifics specifics,
    syncer::CollaborationMetadata collaboration_metadata,
    base::Time creation_time) {
  CHECK(!collaboration_metadata.collaboration_id()->empty());

  if (specifics.has_tab()) {
    // Similar to saved tab groups, if the tab URL is not valid for sharing,
    // change it to the Chrome unsupported URL before sending it to the server.
    // The local db will still store the original URL for session restoration.
    if (!IsURLValidForSavedTabGroups(GURL(specifics.tab().url()))) {
      sync_pb::SharedTab* tab = specifics.mutable_tab();
      tab->set_url(kChromeSavedTabGroupUnsupportedURL);
      tab->clear_title();
    }
  }
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specifics.guid();
  *entity_data->specifics.mutable_shared_tab_group_data() =
      std::move(specifics);
  entity_data->collaboration_metadata = std::move(collaboration_metadata);
  entity_data->creation_time = creation_time;
  return entity_data;
}

void AddEntryToBatch(syncer::MutableDataBatch* batch,
                     sync_pb::SharedTabGroupDataSpecifics specifics,
                     const syncer::CollaborationId& collaboration_id,
                     base::Time creation_time,
                     const GaiaId& changed_by) {
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(std::move(specifics),
                       syncer::CollaborationMetadata::ForLocalChange(
                           changed_by, collaboration_id),
                       creation_time);

  // Copy because our key is the name of `entity_data`.
  std::string name = entity_data->name;

  batch->Put(name, std::move(entity_data));
}

syncer::CollaborationMetadata ExtractCollaborationMetadata(
    const syncer::EntityMetadataMap& sync_metadata,
    const std::string& storage_key) {
  auto it = sync_metadata.find(storage_key);
  if (it == sync_metadata.end()) {
    return syncer::CollaborationMetadata::FromLocalProto(
        sync_pb::EntityMetadata::CollaborationMetadata());
  }

  return syncer::CollaborationMetadata::FromLocalProto(
      it->second->collaboration());
}

// Tries to parse the unique position from the metadata. Returns an invalid
// position if the metadata is not found.
syncer::UniquePosition ParseUniquePositionFromMetadata(
    const syncer::EntityMetadataMap& sync_metadata,
    const std::string& storage_key) {
  auto it = sync_metadata.find(storage_key);
  if (it == sync_metadata.end()) {
    return syncer::UniquePosition();
  }

  return syncer::UniquePosition::FromProto(it->second->unique_position());
}

base::Time ExtractCreationTimeFromMetadata(
    const syncer::EntityMetadataMap& sync_metadata,
    const std::string& storage_key) {
  auto it = sync_metadata.find(storage_key);
  if (it == sync_metadata.end()) {
    return base::Time();
  }

  return syncer::ProtoTimeToTime(it->second->creation_time());
}

base::Time ExtractModificationTimeFromMetadata(
    const syncer::EntityMetadataMap& sync_metadata,
    const std::string& storage_key) {
  auto it = sync_metadata.find(storage_key);
  if (it == sync_metadata.end()) {
    return base::Time();
  }

  return syncer::ProtoTimeToTime(it->second->modification_time());
}

// Sorts stored entries by their unique position. The resulting order is:
// 1. Tabs with valid unique positions, ordered by their unique position.
// 2. Tabs with invalid unique positions, ordered by their update time.
// 3. Groups preserved in the original order.
void SortStoredEntriesByUniquePosition(
    std::vector<proto::SharedTabGroupData>& stored_entries,
    const syncer::EntityMetadataMap& sync_metadata) {
  std::ranges::stable_sort(
      stored_entries, [&sync_metadata](const proto::SharedTabGroupData& left,
                                       const proto::SharedTabGroupData& right) {
        // Tabs are sorted before groups.
        if (left.specifics().has_tab() != right.specifics().has_tab()) {
          return left.specifics().has_tab();
        }

        // Compare tabs by their unique position.
        if (left.specifics().has_tab() && right.specifics().has_tab()) {
          syncer::UniquePosition left_unique_position =
              ParseUniquePositionFromMetadata(sync_metadata,
                                              left.specifics().guid());
          syncer::UniquePosition right_unique_position =
              ParseUniquePositionFromMetadata(sync_metadata,
                                              right.specifics().guid());
          if (left_unique_position.IsValid() !=
              right_unique_position.IsValid()) {
            return left_unique_position.IsValid();
          }
          if (left_unique_position.IsValid()) {
            return left_unique_position.LessThan(right_unique_position);
          }

          // Order tabs with invalid unique positions by their update time for
          // consistency.
          return left.specifics().update_time_windows_epoch_micros() <
                 right.specifics().update_time_windows_epoch_micros();
        }

        // Consider all the groups equal.
        return false;
      });
}

// Parses stored entries and populates the result to the `on_load_callback`.
// Returns tabs missing groups.
std::vector<std::tuple<sync_pb::SharedTabGroupDataSpecifics,
                       syncer::CollaborationMetadata,
                       base::Time,
                       base::Time>>
LoadStoredEntries(std::vector<proto::SharedTabGroupData> stored_entries,
                  SyncBridgeTabGroupModelWrapper* model_wrapper,
                  const syncer::EntityMetadataMap& sync_metadata) {
  DVLOG(2) << "Loading SharedTabGroupData entries from the disk: "
           << stored_entries.size();

  std::vector<SavedTabGroup> groups;
  std::unordered_map<std::string, size_t> group_guid_to_next_tab_position;

  // `stored_entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist yet.
  for (const proto::SharedTabGroupData& proto : stored_entries) {
    const sync_pb::SharedTabGroupDataSpecifics& specifics = proto.specifics();
    if (!specifics.has_tab_group()) {
      continue;
    }
    // Collaboration ID is stored as part of sync metadata.
    const std::string& storage_key = specifics.guid();
    syncer::CollaborationMetadata collaboration_metadata =
        ExtractCollaborationMetadata(sync_metadata, storage_key);
    if (collaboration_metadata.collaboration_id()->empty()) {
      stats::RecordSharedTabGroupDataLoadFromDiskResult(
          stats::SharedTabGroupDataLoadFromDiskResult::kMissingCollaborationId);
      continue;
    }
    SavedTabGroup group = SpecificsToSharedTabGroup(
        specifics, collaboration_metadata,
        ExtractCreationTimeFromMetadata(sync_metadata, storage_key),
        proto.local_group_data().use_originating_tab_group_guid());
    // Load remaining local-only fields.
    if (AreLocalIdsPersisted() &&
        proto.local_group_data().has_local_group_id()) {
      group.SetLocalGroupId(
          LocalTabGroupIDFromString(proto.local_group_data().local_group_id()));
    }
    if (proto.local_group_data().has_is_transitioning_to_saved()) {
      group.SetIsTransitioningToSaved(
          proto.local_group_data().is_transitioning_to_saved());
    }
    if (proto.local_group_data().has_is_group_hidden()) {
      group.SetIsHidden(proto.local_group_data().is_group_hidden());
    }
    stats::RecordSharedTabGroupDataLoadFromDiskResult(
        stats::SharedTabGroupDataLoadFromDiskResult::kSuccess);
    groups.emplace_back(std::move(group));

    // There should not be duplicate group GUIDs because they are used as
    // storage keys.
    group_guid_to_next_tab_position.emplace(specifics.guid(), 0);
  }

  // Order all entries by their unique position. Do not distinguish between
  // groups and tabs for simplicity (groups don't have unique positions) and
  // order tabs with invalid unique positions to the end.
  SortStoredEntriesByUniquePosition(stored_entries, sync_metadata);

  // Parse tabs and find tabs missing groups. This code relies on the order of
  // the tab entries to calculate tab positions.
  std::vector<std::tuple<sync_pb::SharedTabGroupDataSpecifics,
                         syncer::CollaborationMetadata, base::Time, base::Time>>
      tabs_missing_groups;
  std::vector<SavedTabGroupTab> tabs;
  for (const proto::SharedTabGroupData& proto : stored_entries) {
    const sync_pb::SharedTabGroupDataSpecifics& specifics = proto.specifics();
    if (!specifics.has_tab()) {
      continue;
    }
    const std::string& storage_key = specifics.guid();
    syncer::CollaborationMetadata collaboration_metadata =
        ExtractCollaborationMetadata(sync_metadata, storage_key);
    if (collaboration_metadata.collaboration_id()->empty()) {
      stats::RecordSharedTabGroupDataLoadFromDiskResult(
          stats::SharedTabGroupDataLoadFromDiskResult::kMissingCollaborationId);
      continue;
    }
    stats::RecordSharedTabGroupDataLoadFromDiskResult(
        stats::SharedTabGroupDataLoadFromDiskResult::kSuccess);

    base::Time creation_time =
        ExtractCreationTimeFromMetadata(sync_metadata, storage_key);
    base::Time modification_time =
        ExtractModificationTimeFromMetadata(sync_metadata, storage_key);
    if (!group_guid_to_next_tab_position.contains(
            specifics.tab().shared_tab_group_guid())) {
      tabs_missing_groups.emplace_back(specifics, collaboration_metadata,
                                       creation_time, modification_time);
      continue;
    }

    size_t tab_position =
        group_guid_to_next_tab_position[specifics.tab()
                                            .shared_tab_group_guid()];
    tabs.emplace_back(SpecificsToSharedTabGroupTab(
        specifics, collaboration_metadata, tab_position, creation_time,
        modification_time,
        /*sanitize_url_and_title=*/false));
    group_guid_to_next_tab_position[specifics.tab().shared_tab_group_guid()]++;
  }

  model_wrapper->Initialize(std::move(groups), std::move(tabs));
  return tabs_missing_groups;
}

void StoreSharedTab(syncer::DataTypeStore::WriteBatch& write_batch,
                    sync_pb::SharedTabGroupDataSpecifics specifics) {
  if (specifics.has_tab()) {
    // Unique position is stored in the sync metadata, so it should not be
    // stored in specifics on the disk.
    specifics.mutable_tab()->clear_unique_position();
  }
  std::string storage_key = specifics.guid();
  proto::SharedTabGroupData local_proto;
  *local_proto.mutable_specifics() = std::move(specifics);
  write_batch.WriteData(storage_key, local_proto.SerializeAsString());
}

void StoreSharedGroup(syncer::DataTypeStore::WriteBatch& write_batch,
                      sync_pb::SharedTabGroupDataSpecifics specifics,
                      proto::LocalSharedTabGroupData local_group_data) {
  std::string storage_key = specifics.guid();
  proto::SharedTabGroupData local_proto;
  *local_proto.mutable_specifics() = std::move(specifics);
  *local_proto.mutable_local_group_data() = std::move(local_group_data);
  write_batch.WriteData(storage_key, local_proto.SerializeAsString());
}

proto::LocalSharedTabGroupData GroupToLocalOnlyData(
    const SavedTabGroup& group) {
  proto::LocalSharedTabGroupData local_group_data;
  if (AreLocalIdsPersisted() && group.local_group_id().has_value()) {
    local_group_data.set_local_group_id(
        LocalTabGroupIDToString(group.local_group_id().value()));
  }
  local_group_data.set_is_transitioning_to_saved(
      group.is_transitioning_to_saved());
  local_group_data.set_is_group_hidden(group.is_hidden());
  local_group_data.set_use_originating_tab_group_guid(
      group.use_originating_tab_group_guid());
  return local_group_data;
}

std::string StorageKeyForTab(const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString();
}

std::string StorageKeyForGroup(const SavedTabGroup& group) {
  return group.saved_guid().AsLowercaseString();
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
  std::ranges::sort(tab_changes, &ReversedUniquePositionComparison);
}

}  // namespace

SharedTabGroupDataSyncBridge::SharedTabGroupDataSyncBridge(
    SyncBridgeTabGroupModelWrapper* model_wrapper,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    PrefService* pref_service,
    data_sharing::Logger* logger)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      model_wrapper_(model_wrapper),
      did_enable_shared_tab_groups_in_last_session_(pref_service->GetBoolean(
          prefs::kDidEnableSharedTabGroupsInLastSession)),
      logger_(logger) {
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
  model_wrapper_->OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType::kInitialMerge);

  // This data type does not have local data and hence there is nothing to
  // merge.
  std::optional<syncer::ModelError> result = ApplyIncrementalSyncChanges(
      std::move(metadata_change_list), std::move(entity_data));

  model_wrapper_->OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType::kCompletedInitialMergeThisSession);
  return result;
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do not store the write batch on destroy by default. This is only required
  // while processing remote updates because it reports an error after the
  // scoped write batch is destroyed. There must not be reentrant calls to
  // ApplyIncrementalSyncChanges().
  CHECK(!ongoing_write_batch_);
  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(
          /*store_write_batch_on_destroy=*/false);
  CHECK(ongoing_write_batch_);

  std::vector<std::unique_ptr<syncer::EntityChange>> delete_changes;

  std::vector<std::unique_ptr<syncer::EntityChange>> tab_updates;
  for (std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        delete_changes.push_back(std::move(change));
        break;
      }
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(change->data().collaboration_metadata.has_value());
        if (change->data().specifics.shared_tab_group_data().has_tab_group()) {
          if (std::optional<syncer::ModelError> error = AddGroupToLocalStorage(
                  change->data().specifics.shared_tab_group_data(),
                  change->data().collaboration_metadata.value(),
                  change->data().creation_time, metadata_change_list.get(),
                  *ongoing_write_batch_)) {
            return error;
          }
        } else if (change->data().specifics.shared_tab_group_data().has_tab()) {
          // Postpone tab updates until all remote groups are added.
          tab_updates.push_back(std::move(change));
        }
        // Ignore entities not having a tab or a group.
        break;
      }
    }
  }

  // Process group and tab deletions first.
  for (const std::unique_ptr<syncer::EntityChange>& change : delete_changes) {
    GaiaId last_updated_by;
    if (change->data().collaboration_metadata) {
      last_updated_by =
          change->data().collaboration_metadata->last_updated_by();
    }
    DeleteDataFromLocalStorage(change->storage_key(),
                               std::move(last_updated_by),
                               *ongoing_write_batch_);
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
    const bool inserted =
        tab_ids_with_pending_model_update
            .insert(base::Uuid::ParseLowercase(
                change->data().specifics.shared_tab_group_data().guid()))
            .second;
    if (!inserted) {
      // The processor guarantees that there is only one update per client tag.
      // Hence, duplicate GUIDs must have different collaboration IDs which
      // should never happen.
      return syncer::ModelError(
          FROM_HERE, syncer::ModelError::Type::kSharedTabGroupDuplicateTabGuid);
    }
  }

  // Process tab updates after applying deletions so that tab updates having
  // deleted groups will be stored to `tabs_missing_groups_`.
  for (const std::unique_ptr<syncer::EntityChange>& change : tab_updates) {
    if (std::optional<syncer::ModelError> error = ApplyRemoteTabUpdate(
            change->data().specifics.shared_tab_group_data(),
            metadata_change_list.get(), *ongoing_write_batch_,
            tab_ids_with_pending_model_update,
            change->data().collaboration_metadata.value(),
            change->data().creation_time, change->data().modification_time)) {
      return error;
    }

    // The tab update has been applied to the model.
    tab_ids_with_pending_model_update.erase(base::Uuid::ParseLowercase(
        change->data().specifics.shared_tab_group_data().guid()));
  }

  // Note that ResolveTabsMissingGroups() must be called after all the tab
  // updates are applied to the model to correctly handle unique positions.
  if (std::optional<syncer::ModelError> error =
          ResolveTabsMissingGroups(*metadata_change_list)) {
    return error;
  }

  ongoing_write_batch_->TakeMetadataChangesFrom(
      std::move(metadata_change_list));

  // Successfully applied all the changes. Explicitly destroy the write batch
  // and store data to the store.
  DestroyOngoingWriteBatch(/*store_write_batch_on_destroy=*/true);

  // Notify the model on committed tab groups.
  ProcessCommittedTabGroups();

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
                      group->collaboration_id().value(), group->creation_time(),
                      group->shared_attribution().updated_by);
    }
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      if (parsed_guids.contains(tab.saved_tab_guid())) {
        AddEntryToBatch(
            batch.get(),
            SharedTabGroupTabToSpecifics(
                tab, change_processor()->GetUniquePositionForStorageKey(
                         StorageKeyForTab(tab))),
            group->collaboration_id().value(), group->creation_time(),
            tab.shared_attribution().updated_by);
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
                    group->collaboration_id().value(), group->creation_time(),
                    group->shared_attribution().updated_by);
    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      AddEntryToBatch(
          batch.get(),
          SharedTabGroupTabToSpecifics(
              tab, change_processor()->GetUniquePositionForStorageKey(
                       StorageKeyForTab(tab))),
          group->collaboration_id().value(), group->creation_time(),
          tab.shared_attribution().updated_by);
    }
  }

  for (const auto& [tab_guid, tab_missing_group] : tabs_missing_groups_) {
    AddEntryToBatch(batch.get(), tab_missing_group.specifics,
                    tab_missing_group.collaboration_metadata.collaboration_id(),
                    tab_missing_group.creation_time,
                    tab_missing_group.collaboration_metadata.last_updated_by());
  }
  return batch;
}

std::string SharedTabGroupDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.collaboration_metadata.has_value());
  return entity_data.specifics.shared_tab_group_data().guid() + "|" +
         entity_data.collaboration_metadata->collaboration_id().value();
}

std::string SharedTabGroupDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
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
  model_wrapper_->OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType::kDisableSync);

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

  tab_groups_waiting_for_commit_.clear();

  // Delete all shared tabs and sync metadata from the store.
  // `delete_metadata_change_list` is not used because all the metadata is
  // deleted anyway.
  store_->DeleteAllDataAndMetadata(base::DoNothing());

  model_wrapper_->OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType::kCompletedDisableSyncThisSession);
}

sync_pb::EntitySpecifics
SharedTabGroupDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::SharedTabGroupDataSpecifics trimmed_specifics =
      entity_specifics.shared_tab_group_data();
  trimmed_specifics.clear_guid();
  trimmed_specifics.clear_update_time_windows_epoch_micros();
  trimmed_specifics.clear_version();

  if (trimmed_specifics.has_tab()) {
    sync_pb::SharedTab* tab = trimmed_specifics.mutable_tab();
    tab->clear_url();
    tab->clear_title();
    tab->clear_shared_tab_group_guid();
    tab->clear_unique_position();

    if (tab->ByteSizeLong() == 0) {
      trimmed_specifics.clear_tab();
    }
  }

  if (trimmed_specifics.has_tab_group()) {
    sync_pb::SharedTabGroup* tab_group = trimmed_specifics.mutable_tab_group();
    tab_group->clear_title();
    tab_group->clear_color();
    tab_group->clear_originating_tab_group_guid();

    if (tab_group->ByteSizeLong() == 0) {
      trimmed_specifics.clear_tab_group();
    }
  }
  // LINT.ThenChange(//components/sync/protocol/shared_tab_group_data_specifics.proto:SharedTabGroupDataSpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_shared_tab_group_data() =
        std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

bool SharedTabGroupDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!entity_data.collaboration_metadata.has_value() ||
      entity_data.collaboration_metadata->collaboration_id()->empty()) {
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
  if (!IsReadyToSync()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupAddedLocally called while not initialized";
    return;
  }

  const SavedTabGroup* group = model_wrapper_->GetGroup(guid);
  CHECK(group);
  CHECK(group->is_shared_tab_group());

  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);
  CHECK(group->collaboration_id().has_value());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      SharedTabGroupToSpecifics(*group);
  StoreSharedGroup(*ongoing_write_batch_, group_specifics,
                   GroupToLocalOnlyData(*group));
  SendToSync(group_specifics,
             syncer::CollaborationMetadata::ForLocalChange(
                 group->shared_attribution().updated_by,
                 group->collaboration_id().value()),
             group->creation_time(),
             ongoing_write_batch_->GetMetadataChangeList());
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
    StoreSharedGroup(*ongoing_write_batch_, tab_specifics,
                     GroupToLocalOnlyData(*group));

    // Pending NTP should never be created for locally added groups.
    CHECK(!tab.is_pending_ntp());
    SendToSync(tab_specifics,
               syncer::CollaborationMetadata::ForLocalChange(
                   tab.shared_attribution().updated_by,
                   group->collaboration_id().value()),
               tab.creation_time(),
               ongoing_write_batch_->GetMetadataChangeList());
  }

  if (group->is_transitioning_to_shared()) {
    // The group needs to be notified when committed to the server.
    tab_groups_waiting_for_commit_.emplace_back(group->saved_guid());
  }
}

void SharedTabGroupDataSyncBridge::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsReadyToSync()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupUpdatedLocally called while not initialized";
    return;
  }

  // The bridge must be called for the shared tab groups only which is
  // guaranteed by the TabGroupSyncBridgeMediator.
  const SavedTabGroup* group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);
  CHECK(group->is_shared_tab_group());

  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);
  if (tab_guid.has_value()) {
    // The tab has been updated, added or removed.
    ProcessTabLocalChange(*group, tab_guid.value(), *ongoing_write_batch_);
  } else {
    // Only group metadata has been updated.
    sync_pb::SharedTabGroupDataSpecifics specifics =
        SharedTabGroupToSpecifics(*group);
    StoreSharedGroup(*ongoing_write_batch_, specifics,
                     GroupToLocalOnlyData(*group));
    SendToSync(specifics,
               syncer::CollaborationMetadata::ForLocalChange(
                   group->shared_attribution().updated_by,
                   group->collaboration_id().value()),
               group->creation_time(),
               ongoing_write_batch_->GetMetadataChangeList());
  }
}

void SharedTabGroupDataSyncBridge::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsReadyToSync()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupRemovedLocally called while not initialized";
    std::erase(tab_groups_waiting_for_commit_, removed_group.saved_guid());
    return;
  }

  CHECK(removed_group.is_shared_tab_group());

  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);

  RemoveEntitySpecifics(removed_group.saved_guid(), *ongoing_write_batch_);
  for (const SavedTabGroupTab& tab : removed_group.saved_tabs()) {
    RemoveEntitySpecifics(tab.saved_tab_guid(), *ongoing_write_batch_);
  }

  std::erase(tab_groups_waiting_for_commit_, removed_group.saved_guid());
}

void SharedTabGroupDataSyncBridge::ProcessTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsReadyToSync()) {
    // Ignore any changes before the model is successfully initialized.
    DVLOG(2) << "SavedTabGroupLocalIdChanged called while not initialized";
    return;
  }

  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);

  const SavedTabGroup* const group = model_wrapper_->GetGroup(group_guid);
  CHECK(group);

  StoreSharedGroup(*ongoing_write_batch_, SharedTabGroupToSpecifics(*group),
                   GroupToLocalOnlyData(*group));
}

void SharedTabGroupDataSyncBridge::UntrackEntitiesForCollaboration(
    const syncer::CollaborationId& collaboration_id) {
  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);

  for (const SavedTabGroup* group : model_wrapper_->GetTabGroups()) {
    if (!group->collaboration_id().has_value()) {
      continue;
    }

    if (group->collaboration_id().value() != collaboration_id) {
      continue;
    }

    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      std::string storage_key = StorageKeyForTab(tab);
      ongoing_write_batch_->GetMetadataChangeList()->ClearMetadata(storage_key);
      change_processor()->UntrackEntityForStorageKey(storage_key);
    }
    std::string storage_key = StorageKeyForGroup(*group);
    ongoing_write_batch_->GetMetadataChangeList()->ClearMetadata(storage_key);
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
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
      stats::RecordSharedTabGroupDataLoadFromDiskResult(
          stats::SharedTabGroupDataLoadFromDiskResult::kFailedToParse);
      continue;
    }
    if (proto.specifics().guid() != r.id) {
      // GUID is used as a storage key, so it should always match.
      stats::RecordSharedTabGroupDataLoadFromDiskResult(
          stats::SharedTabGroupDataLoadFromDiskResult::kUnexpectedGuid);
      continue;
    }
    if (!proto.specifics().has_tab_group() && !proto.specifics().has_tab()) {
      stats::RecordSharedTabGroupDataLoadFromDiskResult(
          stats::SharedTabGroupDataLoadFromDiskResult::kMissingGroupAndTab);
      continue;
    }
    stored_entries.emplace_back(std::move(proto));
  }

  // Check if this is the first time shared tab groups is enabled and we need
  // to do some migrations.
  if (!did_enable_shared_tab_groups_in_last_session_) {
    FixLocalTabGroupIDsForSharedGroupsDuringFeatureEnabling(stored_entries);
  }

  std::vector<std::tuple<sync_pb::SharedTabGroupDataSpecifics,
                         syncer::CollaborationMetadata, base::Time, base::Time>>
      loaded_tabs_missing_groups =
          LoadStoredEntries(std::move(stored_entries), model_wrapper_,
                            metadata_batch->GetAllMetadata());
  for (auto& [specifics, collaboration_metadata, creation_time,
              modification_time] : loaded_tabs_missing_groups) {
    base::Uuid tab_guid = base::Uuid::ParseLowercase(specifics.guid());
    CHECK(tab_guid.is_valid());
    tabs_missing_groups_.insert_or_assign(
        std::move(tab_guid),
        TabMissingGroup(std::move(specifics), std::move(collaboration_metadata),
                        creation_time, modification_time));
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SharedTabGroupDataSyncBridge::
    FixLocalTabGroupIDsForSharedGroupsDuringFeatureEnabling(
        std::vector<proto::SharedTabGroupData>& stored_entries) {
  base::ScopedClosureRunner write_batch_scoped_destroy_closure =
      CreateWriteBatchWithDestroyClosure(/*store_write_batch_on_destroy=*/true);
  CHECK(ongoing_write_batch_);

  for (proto::SharedTabGroupData& proto : stored_entries) {
    if (!proto.specifics().has_tab_group()) {
      continue;
    }

    if (!proto.has_local_group_data() ||
        !proto.local_group_data().has_local_group_id()) {
      continue;
    }

    // At this point, this is a tab group and it has non-empty local ID.
    // In the last session, the shared tab group feature was not enabled, hence
    // it must be a left over from an earlier feature rollback. Clear it as the
    // group in local tab model must have been closed by now in the earlier
    // session.
    proto.mutable_local_group_data()->clear_local_group_id();
    ongoing_write_batch_->WriteData(proto.specifics().guid(),
                                    proto.SerializeAsString());
  }
}

void SharedTabGroupDataSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(
        {FROM_HERE,
         syncer::ModelError::Type::kSharedTabGroupDataDatabaseSaveFailed});
  }
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::AddGroupToLocalStorage(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const syncer::CollaborationMetadata& collaboration_metadata,
    base::Time creation_time,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::DataTypeStore::WriteBatch& write_batch) {
  base::Uuid group_guid = base::Uuid::ParseLowercase(specifics.guid());
  if (!group_guid.is_valid()) {
    // Ignore remote updates having invalid data.
    return std::nullopt;
  }

  CHECK(specifics.has_tab_group());

  if (!model_wrapper_->GetGroup(group_guid)) {
    // This is a new remotely created group. Add the group from sync into local
    // storage. Note that on some platforms new remote groups may open in the
    // tab strip, and associate its local group ID. This is currently prevented
    // by delaying observer calls in the TabGroupSyncService.
    StoreSharedGroup(write_batch, specifics, proto::LocalSharedTabGroupData());
    bool use_originating_tab_group_guid = collaboration_metadata.created_by() ==
                                          change_processor()->TrackedGaiaId();
    model_wrapper_->AddGroup(SpecificsToSharedTabGroup(
        specifics, collaboration_metadata, creation_time,
        use_originating_tab_group_guid));
    return std::nullopt;
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
              specifics.update_time_windows_epoch_micros()),
          collaboration_metadata.last_updated_by());
  CHECK(existing_group);

  // TODO(crbug.com/381540386): move this check before the merge.
  if (existing_group->collaboration_id() !=
      collaboration_metadata.collaboration_id()) {
    // Shared tab groups should never change collaboration IDs.
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::
                       kSharedTabGroupUnexpectedCollaborationIdForGroup);
  }

  // Create new specifics in case some fields were merged.
  sync_pb::SharedTabGroupDataSpecifics updated_specifics =
      SharedTabGroupToSpecifics(*existing_group);

  StoreSharedGroup(write_batch, updated_specifics,
                   GroupToLocalOnlyData(*existing_group));

  return std::nullopt;
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::ApplyRemoteTabUpdate(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    syncer::MetadataChangeList* metadata_change_list,
    syncer::DataTypeStore::WriteBatch& write_batch,
    const std::set<base::Uuid>& tab_ids_with_pending_model_update,
    const syncer::CollaborationMetadata& collaboration_metadata,
    base::Time creation_time,
    base::Time modification_time) {
  CHECK(specifics.has_tab());

  base::Uuid tab_guid = base::Uuid::ParseLowercase(specifics.guid());
  CHECK(tab_guid.is_valid());
  base::Uuid group_guid =
      base::Uuid::ParseLowercase(specifics.tab().shared_tab_group_guid());
  if (!group_guid.is_valid()) {
    // Ignore tab with invalid data.
    return std::nullopt;
  }

  const SavedTabGroup* existing_group = model_wrapper_->GetGroup(group_guid);
  if (!existing_group) {
    // The tab does not have a corresponding group. This can happen when sync
    // sends the tab data before the group data. In this case, the tab is stored
    // in case the group comes in later.
    tabs_missing_groups_.insert_or_assign(
        tab_guid, TabMissingGroup(specifics, collaboration_metadata,
                                  creation_time, modification_time));
    StoreSharedTab(write_batch, specifics);
    return std::nullopt;
  }

  if (existing_group->collaboration_id() !=
      syncer::CollaborationId(collaboration_metadata.collaboration_id())) {
    // Shared tabs must have the same collaboration ID as their group.
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::
                       kSharedTabGroupUnexpectedCollaborationIdForTab);
  }

  if (existing_group->ContainsTab(tab_guid)) {
    const std::optional<int> current_tab_index =
        existing_group->GetIndexOfTab(tab_guid);
    CHECK(current_tab_index.has_value());
    const size_t position_insert_before = PositionToInsertRemoteTab(
        specifics.tab().unique_position(), *existing_group,
        tab_ids_with_pending_model_update);

    const SavedTabGroupTab* merged_tab =
        model_wrapper_->MergeRemoteTab(SpecificsToSharedTabGroupTab(
            specifics, collaboration_metadata,
            AdjustPreferredTabIndex(position_insert_before,
                                    current_tab_index.value()),
            creation_time, modification_time,
            /*sanitize_url_and_title=*/true));

    // Unique positions are stored by sync in sync metadata.
    sync_pb::SharedTabGroupDataSpecifics merged_entry =
        SharedTabGroupTabToSpecifics(*merged_tab, sync_pb::UniquePosition());

    // Write result to the store.
    StoreSharedTab(write_batch, std::move(merged_entry));
    return std::nullopt;
  }

  // Tabs are stored to the local storage regardless of the existence of its
  // group in order to recover the tabs in the event the group was not received
  // and a crash / restart occurred.
  StoreSharedTab(write_batch, specifics);

  // This is a new tab for the group.
  model_wrapper_->AddTabToGroup(
      existing_group->saved_guid(),
      SpecificsToSharedTabGroupTab(
          specifics, collaboration_metadata,
          PositionToInsertRemoteTab(specifics.tab().unique_position(),
                                    *existing_group,
                                    tab_ids_with_pending_model_update),
          creation_time, modification_time,
          /*sanitize_url_and_title=*/true));

  return std::nullopt;
}

void SharedTabGroupDataSyncBridge::DeleteDataFromLocalStorage(
    const std::string& storage_key,
    GaiaId removed_by,
    syncer::DataTypeStore::WriteBatch& write_batch) {
  write_batch.DeleteData(storage_key);

  base::Uuid guid = base::Uuid::ParseLowercase(storage_key);
  if (!guid.is_valid()) {
    return;
  }

  // Check if the model contains the group guid. If so, remove that group and
  // all of its tabs.
  if (model_wrapper_->GetGroup(guid)) {
    std::erase(tab_groups_waiting_for_commit_, guid);
    model_wrapper_->RemoveGroup(guid);
    return;
  }

  if (const SavedTabGroup* group_containing_tab =
          model_wrapper_->GetGroupContainingTab(guid)) {
    model_wrapper_->RemoveTabFromGroup(group_containing_tab->saved_guid(), guid,
                                       std::move(removed_by));
  }
}

void SharedTabGroupDataSyncBridge::SendToSync(
    sync_pb::SharedTabGroupDataSpecifics specific,
    syncer::CollaborationMetadata collaboration_metadata,
    base::Time creation_time,
    syncer::MetadataChangeList* metadata_change_list) {
  CHECK(metadata_change_list);
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<syncer::EntityData> entity_data = CreateEntityData(
      std::move(specific), std::move(collaboration_metadata), creation_time);

  // Copy because our key is the name of `entity_data`.
  std::string storage_key = GetStorageKey(*entity_data);
  change_processor()->Put(storage_key, std::move(entity_data),
                          metadata_change_list);
}

void SharedTabGroupDataSyncBridge::ProcessTabLocalChange(
    const SavedTabGroup& group,
    const base::Uuid& tab_id,
    syncer::DataTypeStore::WriteBatch& write_batch) {
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
  const SavedTabGroupTab& tab = group.saved_tabs()[tab_index.value()];
  sync_pb::SharedTabGroupDataSpecifics specifics = SharedTabGroupTabToSpecifics(
      tab, CalculateUniquePosition(group, tab_index.value()));
  StoreSharedTab(write_batch, specifics);
  // Pending NTP should never be synced, only be stored locally.
  if (tab.is_pending_ntp()) {
    return;
  }
  SendToSync(specifics,
             syncer::CollaborationMetadata::ForLocalChange(
                 tab.shared_attribution().updated_by,
                 group.collaboration_id().value()),
             tab.creation_time(), write_batch.GetMetadataChangeList());
}

void SharedTabGroupDataSyncBridge::RemoveEntitySpecifics(
    const base::Uuid& guid,
    syncer::DataTypeStore::WriteBatch& write_batch) {
  write_batch.DeleteData(guid.AsLowercaseString());

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  change_processor()->Delete(guid.AsLowercaseString(),
                             syncer::DeletionOrigin::Unspecified(),
                             write_batch.GetMetadataChangeList());
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

base::ScopedClosureRunner
SharedTabGroupDataSyncBridge::CreateWriteBatchWithDestroyClosure(
    bool store_write_batch_on_destroy) {
  if (ongoing_write_batch_) {
    // There is an ongoing write batch, hence do not create a new one and do not
    // destroy the existing one in the current scope.
    return base::ScopedClosureRunner(base::DoNothing());
  }

  // This is not a reentrant call, create a new write batch and return a scoped
  // closure runner that will destroy it when it goes out of scope.
  ongoing_write_batch_ = store_->CreateWriteBatch();
  return base::ScopedClosureRunner(base::BindOnce(
      &SharedTabGroupDataSyncBridge::DestroyOngoingWriteBatch,
      weak_ptr_factory_.GetWeakPtr(), store_write_batch_on_destroy));
}

void SharedTabGroupDataSyncBridge::DestroyOngoingWriteBatch(
    bool store_write_batch_on_destroy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the bridge is in the error state, the write batch should not be
  // committed to avoid storing invalid state. This method may be called when
  // `ongoing_write_batch_` is null but only when `store_write_batch_on_destroy`
  // is false. This is required to be able to commit the write batch explicitly
  // in ApplyIncrementalSyncChanges().
  if (store_write_batch_on_destroy &&
      !change_processor()->GetError().has_value()) {
    CHECK(ongoing_write_batch_);
    store_->CommitWriteBatch(
        std::move(ongoing_write_batch_),
        base::BindOnce(&SharedTabGroupDataSyncBridge::OnDatabaseSave,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  ongoing_write_batch_ = nullptr;
}

bool SharedTabGroupDataSyncBridge::IsReadyToSync() const {
  return model_wrapper_->IsInitialized() &&
         change_processor()->IsTrackingMetadata();
}

void SharedTabGroupDataSyncBridge::ProcessCommittedTabGroups() {
  for (const base::Uuid& group_guid : tab_groups_waiting_for_commit_) {
    const SavedTabGroup* group = model_wrapper_->GetGroup(group_guid);
    if (!group) {
      // The group is somehow erased. Cleanup from other relevant in-memory
      // lists.
      std::erase(tab_groups_waiting_for_commit_, group_guid);
      continue;
    }

    CHECK(group->is_shared_tab_group());

    if (change_processor()->IsEntityUnsynced(StorageKeyForGroup(*group))) {
      // The group is not committed yet, wait for the commit to finish.
      continue;
    }

    for (const SavedTabGroupTab& tab : group->saved_tabs()) {
      if (change_processor()->IsEntityUnsynced(StorageKeyForTab(tab))) {
        // The tab is not committed yet, wait for the commit to finish.
        continue;
      }
    }

    model_wrapper_->MarkTransitionedToShared(group->saved_guid());
    std::erase(tab_groups_waiting_for_commit_, group_guid);
  }
}

std::optional<syncer::ModelError>
SharedTabGroupDataSyncBridge::ResolveTabsMissingGroups(
    syncer::MetadataChangeList& metadata_change_list) {
  // This method should only be called when there is an ongoing write batch,
  // for example during a remote update.
  CHECK(ongoing_write_batch_);
  for (const auto& [tab_guid, tab_missing_group] : tabs_missing_groups_) {
    base::Uuid group_guid = base::Uuid::ParseLowercase(
        tab_missing_group.specifics.tab().shared_tab_group_guid());
    const SavedTabGroup* group = model_wrapper_->GetGroup(group_guid);
    if (!group) {
      // The group still does not exist in the model.
      continue;
    }

    // The group exists in the model, simulate a remote update for the tab. Note
    // that `tab_ids_with_pending_model_update` is empty because all the tabs in
    // the model are already updated (and other tabs missing groups are still
    // not in the model).
    if (std::optional<syncer::ModelError> error =
            ApplyRemoteTabUpdate(tab_missing_group.specifics,
                                 &metadata_change_list, *ongoing_write_batch_,
                                 /*tab_ids_with_pending_model_update=*/{},
                                 tab_missing_group.collaboration_metadata,
                                 tab_missing_group.creation_time,
                                 tab_missing_group.modification_time)) {
      return error;
    }
  }
  return std::nullopt;
}

sync_pb::SharedTabGroupDataSpecifics
SharedTabGroupDataSyncBridge::SharedTabGroupToSpecifics(
    const SavedTabGroup& group) const {
  CHECK(group.is_shared_tab_group());
  // WARNING: all fields need to be set or cleared explicitly.
  // WARNING: if you are adding support for new `SharedTabGroupDataSpecifics`
  // fields, you need to update the following functions accordingly:
  // `TrimAllSupportedFieldsFromRemoteSpecifics`.
  sync_pb::SharedTabGroupDataSpecifics pb_specifics =
      change_processor()
          ->GetPossiblyTrimmedRemoteSpecifics(StorageKeyForGroup(group))
          .shared_tab_group_data();
  pb_specifics.set_guid(group.saved_guid().AsLowercaseString());
  pb_specifics.set_update_time_windows_epoch_micros(
      group.update_time().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::SharedTabGroup* pb_group = pb_specifics.mutable_tab_group();
  pb_group->set_color(TabGroupColorToSyncColor(group.color()));
  pb_group->set_title(base::UTF16ToUTF8(group.title()));

  // Force returning originating tab group GUID for specifics (both local and
  // network).
  if (group.GetOriginatingTabGroupGuid(/*for_sync=*/true).has_value()) {
    pb_group->set_originating_tab_group_guid(
        group.GetOriginatingTabGroupGuid(/*for_sync=*/true)
            .value()
            .AsLowercaseString());
  } else {
    pb_group->clear_originating_tab_group_guid();
  }

  pb_specifics.set_version(kCurrentSharedTabGroupDataSpecificsProtoVersion);
  return pb_specifics;
}

sync_pb::SharedTabGroupDataSpecifics
SharedTabGroupDataSyncBridge::SharedTabGroupTabToSpecifics(
    const SavedTabGroupTab& tab,
    sync_pb::UniquePosition unique_position) const {
  // WARNING: all fields need to be set or cleared explicitly.
  // WARNING: if you are adding support for new `SharedTabGroupDataSpecifics`
  // fields, you need to update the following functions accordingly:
  // `TrimAllSupportedFieldsFromRemoteSpecifics`.
  sync_pb::SharedTabGroupDataSpecifics specifics =
      change_processor()
          ->GetPossiblyTrimmedRemoteSpecifics(StorageKeyForTab(tab))
          .shared_tab_group_data();

  specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  specifics.set_update_time_windows_epoch_micros(
      tab.update_time().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_url(tab.url().spec());
  pb_tab->set_shared_tab_group_guid(tab.saved_group_guid().AsLowercaseString());
  pb_tab->set_title(base::UTF16ToUTF8(tab.title()));
  *pb_tab->mutable_unique_position() = std::move(unique_position);

  specifics.set_version(kCurrentSharedTabGroupDataSpecificsProtoVersion);
  return specifics;
}

SharedTabGroupDataSyncBridge::TabMissingGroup::TabMissingGroup(
    sync_pb::SharedTabGroupDataSpecifics specifics,
    syncer::CollaborationMetadata collaboration_metadata,
    base::Time creation_time,
    base::Time modification_time)
    : specifics(std::move(specifics)),
      collaboration_metadata(std::move(collaboration_metadata)),
      creation_time(creation_time),
      modification_time(modification_time) {}

SharedTabGroupDataSyncBridge::TabMissingGroup::TabMissingGroup(
    const TabMissingGroup& other) = default;

SharedTabGroupDataSyncBridge::TabMissingGroup&
SharedTabGroupDataSyncBridge::TabMissingGroup::operator=(
    const TabMissingGroup& other) = default;

SharedTabGroupDataSyncBridge::TabMissingGroup::TabMissingGroup(
    TabMissingGroup&& other) = default;

SharedTabGroupDataSyncBridge::TabMissingGroup&
SharedTabGroupDataSyncBridge::TabMissingGroup::operator=(
    TabMissingGroup&& other) = default;

SharedTabGroupDataSyncBridge::TabMissingGroup::~TabMissingGroup() = default;

}  // namespace tab_groups
