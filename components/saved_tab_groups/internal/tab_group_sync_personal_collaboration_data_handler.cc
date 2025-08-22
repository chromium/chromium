// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_personal_collaboration_data_handler.h"

#include <optional>

#include "base/uuid.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/saved_tab_groups/internal/personal_collaboration_data_conversion_utils.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace tab_groups {

namespace {

using SpecificsType = ::data_sharing::personal_collaboration_data::
    PersonalCollaborationDataService::SpecificsType;

}  // namespace

TabGroupSyncPersonalCollaborationDataHandler::
    TabGroupSyncPersonalCollaborationDataHandler(
        SavedTabGroupModel* model,
        data_sharing::personal_collaboration_data::
            PersonalCollaborationDataService*
                personal_collaboration_data_service)
    : saved_tab_group_model_(model),
      personal_collaboration_data_service_(
          personal_collaboration_data_service) {
  saved_tab_group_model_observation_.Observe(model);
}

TabGroupSyncPersonalCollaborationDataHandler::
    ~TabGroupSyncPersonalCollaborationDataHandler() = default;

void TabGroupSyncPersonalCollaborationDataHandler::OnInitialized() {
  ApplyPersonalCollaborationData();
}

void TabGroupSyncPersonalCollaborationDataHandler::OnSpecificsUpdated(
    data_sharing::personal_collaboration_data::
        PersonalCollaborationDataService::SpecificsType specifics_type,
    const std::string& storage_key,
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  if (specifics_type == SpecificsType::kSharedTabGroupSpecifics) {
    UpdateTabGroupSpecifics(&specifics);
  } else if (specifics_type == SpecificsType::kSharedTabSpecifics) {
    UpdateTabSpecifics(&specifics);
  }
}

void TabGroupSyncPersonalCollaborationDataHandler::
    OnPersonalCollaborationDataServiceDestroyed() {
  personal_collaboration_data_service_observation_.Reset();
}

void TabGroupSyncPersonalCollaborationDataHandler::
    SavedTabGroupReorderedLocally() {
  for (const SavedTabGroup* group :
       saved_tab_group_model_->GetSharedTabGroupsOnly()) {
    WriteTabGroupDetailToSyncIfPositionChanged(*group);
  }
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(guid);

  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  std::string storage_key = CreateClientTagForSharedGroup(*group);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      personal_collaboration_data_service_->GetSpecifics(
          SpecificsType::kSharedTabGroupSpecifics, storage_key);
  if (specifics.has_value()) {
    UpdateTabGroupSpecifics(&specifics.value());
  }
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(guid);

  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  WriteTabGroupDetailToSyncIfPositionChanged(*group);
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(group_guid);
  CHECK(group);
  if (!group->is_shared_tab_group()) {
    return;
  }

  MaybeRemoveTabDetailsOnGroupUpdate(*group, tab_guid);
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* group = saved_tab_group_model_->Get(group_guid);
  CHECK(group);
  if (!group->is_shared_tab_group()) {
    return;
  }

  if (tab_guid) {
    MaybeRemoveTabDetailsOnGroupUpdate(*group, tab_guid);
  } else {
    // Handle shared tab group details.
    WriteTabGroupDetailToSyncIfPositionChanged(*group);
  }
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  SavedTabGroupRemovedLocally(removed_group);
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  // Remove all specifics.
  if (!removed_group.is_shared_tab_group()) {
    return;
  }

  // Delete tab entities for all tabs in the group.
  for (const SavedTabGroupTab& tab : removed_group.saved_tabs()) {
    personal_collaboration_data_service_->DeleteSpecifics(
        SpecificsType::kSharedTabSpecifics,
        CreateClientTagForSharedTab(removed_group, tab));
  }

  // Remove tab group details entity.
  personal_collaboration_data_service_->DeleteSpecifics(
      SpecificsType::kSharedTabGroupSpecifics,
      CreateClientTagForSharedGroup(removed_group));
}

void TabGroupSyncPersonalCollaborationDataHandler::
    SavedTabGroupTabLastSeenTimeUpdated(const base::Uuid& tab_id,
                                        TriggerSource source) {
  if (source != TriggerSource::LOCAL) {
    return;
  }

  // Look through all tabs in this group and create entities for changes
  // that are not synced.
  const SavedTabGroup* group =
      saved_tab_group_model_->GetGroupContainingTab(tab_id);
  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  const std::optional<base::Time>& model_last_seen = tab->last_seen_time();
  if (!model_last_seen.has_value()) {
    // This tab has not been seen by the user. Avoid syncing tabs
    // without a timestamp by skipping this.
    return;
  }

  const std::string storage_key = CreateClientTagForSharedTab(*group, *tab);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      personal_collaboration_data_service_->GetSpecifics(
          SpecificsType::kSharedTabSpecifics, storage_key);

  if (specifics.has_value()) {
    const base::Time proto_last_seen =
        DeserializeTime(specifics.value()
                            .shared_tab_details()
                            .last_seen_timestamp_windows_epoch());

    if (proto_last_seen >= model_last_seen) {
      // Ignore the value if sync and model are up-to-date. Technically, it
      // should never be true that the model data is older than the value
      // in sync since we update it elsewhere, but this is also ignored.
      return;
    }
  }

  // Write specifics to sync.
  personal_collaboration_data_service_->CreateOrUpdateSpecifics(
      SpecificsType::kSharedTabSpecifics, storage_key,
      base::BindOnce(
          &PopulatePersonalCollaborationSpecificsFromSavedTabGroupTab, *group,
          *tab));
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupModelLoaded() {
  personal_collaboration_data_service_observation_.Observe(
      personal_collaboration_data_service_);
}

void TabGroupSyncPersonalCollaborationDataHandler::
    ApplyPersonalCollaborationData() {
  if (!personal_collaboration_data_service_) {
    return;
  }

  const auto all_specifics =
      personal_collaboration_data_service_->GetAllSpecifics();
  for (const sync_pb::SharedTabGroupAccountDataSpecifics* specifics :
       all_specifics) {
    if (specifics->has_shared_tab_details()) {
      UpdateTabSpecifics(specifics);
    } else if (specifics->has_shared_tab_group_details()) {
      UpdateTabGroupSpecifics(specifics);
    }
  }
}

void TabGroupSyncPersonalCollaborationDataHandler::UpdateTabSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics* specifics) {
  // Can only be called with specifics containing TabDetails.
  CHECK(specifics->has_shared_tab_details());

  const base::Uuid group_id = base::Uuid::ParseCaseInsensitive(
      specifics->shared_tab_details().shared_tab_group_guid());
  const SavedTabGroup* group = saved_tab_group_model_->Get(group_id);
  if (!group) {
    return;
  }

  const base::Uuid tab_id = base::Uuid::ParseCaseInsensitive(specifics->guid());
  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  saved_tab_group_model_->UpdateTabLastSeenTimeFromSync(
      group_id, tab_id,
      DeserializeTime(
          specifics->shared_tab_details().last_seen_timestamp_windows_epoch()));
}

void TabGroupSyncPersonalCollaborationDataHandler::UpdateTabGroupSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics* specifics) {
  CHECK(specifics->has_shared_tab_group_details());

  const base::Uuid group_id =
      base::Uuid::ParseCaseInsensitive(specifics->guid());
  const SavedTabGroup* group = saved_tab_group_model_->Get(group_id);
  if (!group) {
    return;
  }

  // Update its position based on the specifics.
  std::optional<size_t> position;
  if (specifics->shared_tab_group_details().has_pinned_position()) {
    position = specifics->shared_tab_group_details().pinned_position();
  }
  saved_tab_group_model_->UpdatePositionForSharedGroupFromSync(group_id,
                                                               position);
}

void TabGroupSyncPersonalCollaborationDataHandler::
    MaybeRemoveTabDetailsOnGroupUpdate(
        const SavedTabGroup& group,
        const std::optional<base::Uuid>& tab_guid) {
  if (!tab_guid) {
    return;
  }

  const SavedTabGroupTab* tab = group.GetTab(tab_guid.value());
  if (tab) {
    return;
  }

  // This is an update for a shared tab deletion from local. Remove the
  // corresponding entity from sync.
  const std::string storage_key = CreateClientTagForSharedTab(
      group.collaboration_id().value(), tab_guid.value());
  personal_collaboration_data_service_->DeleteSpecifics(
      SpecificsType::kSharedTabSpecifics, storage_key);
}

void TabGroupSyncPersonalCollaborationDataHandler::
    WriteTabGroupDetailToSyncIfPositionChanged(const SavedTabGroup& group) {
  std::string storage_key = CreateClientTagForSharedGroup(group);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      personal_collaboration_data_service_->GetSpecifics(
          SpecificsType::kSharedTabGroupSpecifics, storage_key);

  bool has_changed = false;
  if (specifics.has_value()) {
    std::optional<size_t> specifics_pinned_position;
    if (specifics->has_shared_tab_group_details()) {
      if (specifics->shared_tab_group_details().has_pinned_position()) {
        specifics_pinned_position =
            specifics->shared_tab_group_details().pinned_position();
      }
    }
    if (group.position() != specifics_pinned_position) {
      has_changed = true;
    }
  } else {
    has_changed = true;
  }

  if (has_changed) {
    // Write specifics to sync.
    personal_collaboration_data_service_->CreateOrUpdateSpecifics(
        SpecificsType::kSharedTabGroupSpecifics, storage_key,
        base::BindOnce(
            &PopulatePersonalCollaborationSpecificsFromSharedTabGroup, group));
  }
}

}  // namespace tab_groups
