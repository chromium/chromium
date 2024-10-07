// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SESSION_SERVICE_TAB_GROUP_SYNC_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SESSION_SERVICE_TAB_GROUP_SYNC_OBSERVER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"

class Profile;
class TabStripModel;

namespace tab_groups {

// This class listens and is notified by the the SavedTabGroupModel /
// TabGroupSyncService::Observer. When notified, we check if the TabStripModel
// contains the affected tabs / groups. If not we disregard the changes.
// Otherwise, we will write the changes to disk using the session service. This
// is done to preserve the saved state of SavedTabGroups across sessions.
class SessionServiceTabGroupSyncObserver
    : public SavedTabGroupModelObserver,
      public TabGroupSyncService::Observer {
 public:
  SessionServiceTabGroupSyncObserver(Profile* profile,
                                     TabStripModel* tab_strip_model,
                                     SessionID session_id);
  ~SessionServiceTabGroupSyncObserver() override;

  SessionServiceTabGroupSyncObserver(
      const SessionServiceTabGroupSyncObserver&) = delete;
  SessionServiceTabGroupSyncObserver& operator=(
      const SessionServiceTabGroupSyncObserver&) = delete;

 private:
  // Overridden from tab_groups::SavedTabGroupModelObserver:
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(
      const tab_groups::SavedTabGroup& removed_group) override;

  // Overridden from tab_groups::TabGroupSyncService::Observer
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const tab_groups::LocalTabGroupID& local_id,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupLocalIdChanged(
      const base::Uuid& sync_id,
      const std::optional<LocalTabGroupID>& local_id) override;

  // Helper function to write any changes to the tab group metadata to the disk
  // using the session service.
  void UpdateTabGroupSessionMetadata(
      const std::optional<LocalTabGroupID> local_id,
      std::optional<std::string> sync_id);

  // Profile used to instantiate and listen to the TabGroupSyncService.
  raw_ptr<Profile> profile_ = nullptr;

  // The TabStripModel we should query changes for.
  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;

  // The SessionID used to determine which browser we should write changes to in
  // the session service.
  SessionID session_id_;

  // Observes the SavedTabGroupModel to update the session restore metadata with
  // the correct sync id.
  base::ScopedObservation<tab_groups::SavedTabGroupModel,
                          tab_groups::SavedTabGroupModelObserver>
      saved_tab_group_observation_{this};
};

}  // namespace tab_groups
#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SESSION_SERVICE_TAB_GROUP_SYNC_OBSERVER_H_
