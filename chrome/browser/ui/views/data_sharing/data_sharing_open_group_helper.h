// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_OPEN_GROUP_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_OPEN_GROUP_HELPER_H_

#include "base/scoped_observation.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

class Browser;

// A helper that opens the shared tab group into browser tab strip. When a user
// tried to join a shared tab group via the share link, opens the group into
// browser direcetly if they are already in the group or wait for the group to
// be synced from remote after they press "Open and Join" on the Join dialog.
class DataSharingOpenGroupHelper
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit DataSharingOpenGroupHelper(Browser* browser);
  DataSharingOpenGroupHelper(const DataSharingOpenGroupHelper&) = delete;
  DataSharingOpenGroupHelper& operator=(const DataSharingOpenGroupHelper&) =
      delete;
  ~DataSharingOpenGroupHelper() override;

  // TabGroupSyncService::Observer override.
  // Compare `group_ids` against the collaboration_ids synced from remote. If
  // they match (meaning the syncs are triggered by the user pressing "Join and
  // Open" on the Join dialog) open the tab groups into current window.
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;

  // If group is synced open it otherwise store `group_id` and wait to be synced
  // from remote. Called after users press "Open and Join" from the Join dialog.
  void OpenTabGroupWhenAvailable(std::string group_id);

  // Open the shared tab group in current window (focus if open) if the group
  // the user tries to join from share link is already synced to the local
  // client (namely the user is already in the group). Return true if it is the
  // case.
  bool OpenTabGroupIfSynced(std::string group_id);

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_service_ = nullptr;

  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_service_observation_{this};

  // The group_ids passed from the Join webui. They're the same as
  // SavedTabGroup::collaboration_id for shared tab groups.
  std::set<std::string> group_ids_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_OPEN_GROUP_HELPER_H_
