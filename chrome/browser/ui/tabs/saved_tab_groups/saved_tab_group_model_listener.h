// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

class TabStripModel;
class Profile;

namespace tab_groups {

class TabGroupServiceWrapper;

// Serves to maintain and listen to browsers who contain saved tab groups and
// update the model if a saved tab group was changed.
class SavedTabGroupModelListener : public BrowserListObserver,
                                   TabStripModelObserver {
 public:
  // Used for testing.
  SavedTabGroupModelListener();
  explicit SavedTabGroupModelListener(TabGroupServiceWrapper* wrapper_service,
                                      Profile* profile);
  SavedTabGroupModelListener(const SavedTabGroupModelListener&) = delete;
  SavedTabGroupModelListener& operator=(
      const SavedTabGroupModelListener& other) = delete;
  ~SavedTabGroupModelListener() override;

  // Start ignoring tab added/removed notifications that pertain to this group.
  void PauseTrackingLocalTabGroup(const tab_groups::TabGroupId& group_id);

  // Stop ignoring tab added/removed notifications that pertain to this group.
  void ResumeTrackingLocalTabGroup(const tab_groups::TabGroupId& group_id);

  // New API for local observation.
  // Temporarily pauses local observers for all tab groups. Will ignore all
  // local updates until ResumeLocalObservation is called.
  void PauseLocalObservation();
  // Resumes local observers for all tab groups.
  void ResumeLocalObservation();

  // Start keeping `saved_tab_group` up to date with changes to its
  // corresponding local group.
  void ConnectToLocalTabGroup(
      const SavedTabGroup& saved_tab_group,
      std::map<content::WebContents*, base::Uuid> web_contents_map);

  // Stop updating the saved group corresponding to the local group with id
  // `tab_group_id` when the local group changes.
  void DisconnectLocalTabGroup(tab_groups::TabGroupId tab_group_id);

  // The saved group corresponding to `local_group_id` was removed, so we must
  // remove the local group to match.
  void RemoveLocalGroupFromSync(tab_groups::TabGroupId local_group_id);

  // Updates the local group with id `local_group_id` to match the current state
  // of the saved tab group, if it is open locally.
  void UpdateLocalGroupFromSync(tab_groups::TabGroupId local_group_id);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabGroupAdded(const tab_groups::TabGroupId& group_id) override;
  void OnTabGroupWillBeRemoved(const tab_groups::TabGroupId& group_id) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(std::optional<tab_groups::TabGroupId> group,
                              content::WebContents* contents,
                              int index) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void WillCloseAllTabs(TabStripModel* tab_strip_model) override;

  // Testing Accessors.
  std::unordered_map<tab_groups::TabGroupId,
                     LocalTabGroupListener,
                     tab_groups::TabGroupIdHash>&
  GetLocalTabGroupListenerMapForTesting() {
    return local_tab_group_listeners_;
  }

 private:
  // Create a SavedTabGroup from the corresponding Tab Group in the TabStrip
  // denoted by `group_id`. Also return a mapping of the WebContents in the tab
  // group to their saved tab guid. This mapping will be used in
  // ConnectToLocalTabGroup in order to observe any changes to the tabs over
  // time.
  std::pair<SavedTabGroup, std::map<content::WebContents*, base::Uuid>>
  CreateSavedTabGroupAndTabMapping(const tab_groups::TabGroupId& group_id);

  // The LocalTabGroupListeners for each saved tab group that's currently open.
  std::unordered_map<tab_groups::TabGroupId,
                     LocalTabGroupListener,
                     tab_groups::TabGroupIdHash>
      local_tab_group_listeners_;
  raw_ptr<TabGroupServiceWrapper> wrapper_service_;
  raw_ptr<Profile> profile_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_
