// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_personal_collaboration_data_handler.h"

#include <optional>

#include "base/uuid.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace tab_groups {

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

void TabGroupSyncPersonalCollaborationDataHandler::OnSpecificsUpdated(
    data_sharing::personal_collaboration_data::
        PersonalCollaborationDataService::SpecificsType specifics_type,
    const std::string& storage_key,
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::
    OnPersonalCollaborationDataServiceDestroyed() {
  personal_collaboration_data_service_observation_.Reset();
}

void TabGroupSyncPersonalCollaborationDataHandler::
    SavedTabGroupReorderedLocally() {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::
    SavedTabGroupReorderedFromSync() {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupLocalIdChanged(
    const base::Uuid& saved_group_id) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::
    SavedTabGroupTabLastSeenTimeUpdated(const base::Uuid& tab_id,
                                        TriggerSource source) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::SavedTabGroupModelLoaded() {
  // TODO(haileywang): Implement.
  personal_collaboration_data_service_observation_.Observe(
      personal_collaboration_data_service_);
}

void TabGroupSyncPersonalCollaborationDataHandler::
    OnSyncBridgeUpdateTypeChanged(
        SyncBridgeUpdateType sync_bridge_update_type) {
  // TODO(haileywang): Implement.
}

void TabGroupSyncPersonalCollaborationDataHandler::
    TabGroupTransitioningToSavedRemovedFromSync(
        const base::Uuid& saved_group_id) {
  // TODO(haileywang): Implement.
}

}  // namespace tab_groups
