// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_PERSONAL_COLLABORATION_DATA_HANDLER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_PERSONAL_COLLABORATION_DATA_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"

namespace base {
class Uuid;
}  // namespace base

namespace sync_pb {
class SharedTabGroupAccountDataSpecifics;
}  // namespace sync_pb

namespace tab_groups {

class SavedTabGroup;
class SavedTabGroupModel;

// Handles personal collaboration data updates for saved tab groups. This class
// observes the SavedTabGroupModel and the PersonalCollaborationDataService to
// keep collaboration account data in sync with the tab group data. This class
// is owned by the TabGroupSyncService.
class TabGroupSyncPersonalCollaborationDataHandler
    : public data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService::Observer,
      public SavedTabGroupModelObserver {
 public:
  TabGroupSyncPersonalCollaborationDataHandler(
      SavedTabGroupModel* model,
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService*
              personal_collaboration_data_service);
  ~TabGroupSyncPersonalCollaborationDataHandler() override;

  // Disallow copy/assign.
  TabGroupSyncPersonalCollaborationDataHandler(
      const TabGroupSyncPersonalCollaborationDataHandler&) = delete;
  TabGroupSyncPersonalCollaborationDataHandler& operator=(
      const TabGroupSyncPersonalCollaborationDataHandler&) = delete;

  // PersonalCollaborationDataService::Observer implementation.
  void OnInitialized() override;
  void OnSpecificsUpdated(
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService::SpecificsType specifics_type,
      const std::string& storage_key,
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) override;
  void OnPersonalCollaborationDataServiceDestroyed() override;

  // SavedTabGroupModelObserver implementation.
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  void SavedTabGroupTabLastSeenTimeUpdated(const base::Uuid& tab_id,
                                           TriggerSource source) override;
  void SavedTabGroupModelLoaded() override;

 private:
  void ApplyPersonalCollaborationData();
  void UpdateTabSpecifics(
      const sync_pb::SharedTabGroupAccountDataSpecifics* specifics);
  void UpdateTabGroupSpecifics(
      const sync_pb::SharedTabGroupAccountDataSpecifics* specifics);
  void MaybeRemoveTabDetailsOnGroupUpdate(
      const SavedTabGroup& group,
      const std::optional<base::Uuid>& tab_guid);
  void WriteTabGroupDetailToSyncIfPositionChanged(const SavedTabGroup& group);

  const raw_ptr<SavedTabGroupModel> saved_tab_group_model_;
  const raw_ptr<data_sharing::personal_collaboration_data::
                    PersonalCollaborationDataService>
      personal_collaboration_data_service_;

  base::ScopedObservation<data_sharing::personal_collaboration_data::
                              PersonalCollaborationDataService,
                          data_sharing::personal_collaboration_data::
                              PersonalCollaborationDataService::Observer>
      personal_collaboration_data_service_observation_{this};
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      saved_tab_group_model_observation_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_TAB_GROUP_SYNC_PERSONAL_COLLABORATION_DATA_HANDLER_H_
