// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_

#include "base/uuid.h"
#include "components/tab_groups/tab_group_id.h"

class Browser;

// The API for performing updates to the SavedTabGroup feature.
class SavedTabGroupController {
  // Opens a Saved Tab Group in a specified browser and sets all of the required
  // state in the SavedTabGroupService.
  virtual void OpenSavedTabGroupInBrowser(
      Browser* browser,
      const base::Uuid& saved_group_guid) = 0;

  // Saves a group. Finds the TabGroup by groupid from all browsers, constructs
  // the saved tab group, and starts listening to all tabs.
  virtual void SaveGroup(const tab_groups::TabGroupId& group_id) = 0;

  // Unsaves a group. Finds the group_id in the list of saved tab groups and
  // removes it. Stops Listening to all tabs.
  virtual void UnsaveGroup(const tab_groups::TabGroupId& group_id) = 0;

  // Pauses listening to the Tab Group in the TabStrip, but maintains the
  // connection between the two.
  virtual void PauseTrackingLocalTabGroup(
      const tab_groups::TabGroupId& group_id) = 0;

  // Resumes listening to a paused Tab Group. The WebContentses in the local
  // group must match the order they were in when the group tracking was paused.
  virtual void ResumeTrackingLocalTabGroup(
      const base::Uuid& saved_group_guid,
      const tab_groups::TabGroupId& group_id) = 0;

  // Stops listening to the Tab Group in the TabStrip. Removes the local tab
  // group id and web content tokens.
  virtual void DisconnectLocalTabGroup(
      const tab_groups::TabGroupId& group_id) = 0;

  // Begins listening to the Tab Group in the TabStrip. Adds the local tab group
  // id and web content tokens.
  virtual void ConnectLocalTabGroup(
      const tab_groups::TabGroupId& local_group_id,
      const base::Uuid& saved_group_guid) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_
