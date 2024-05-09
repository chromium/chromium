// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_

#include <unordered_set>

#include "base/uuid.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace tab_groups {

class SavedTabGroupTab;

class SavedTabGroupUtils {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeleteGroupMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMoveGroupToNewWindowMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleGroupPinStateMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabsTitleItem);

  SavedTabGroupUtils() = delete;
  SavedTabGroupUtils(const SavedTabGroupUtils&) = delete;
  SavedTabGroupUtils& operator=(const SavedTabGroupUtils&) = delete;

  static void RemoveGroupFromTabstrip(
      const Browser* browser,
      const tab_groups::TabGroupId& local_group);
  static void UngroupSavedGroup(const Browser* browser,
                                const base::Uuid& saved_group_guid);
  static void DeleteSavedGroup(const Browser* browser,
                               const base::Uuid& saved_group_guid);

  // Open the `url` to the end of `browser` tab strip.
  static void OpenUrlToBrowser(Browser* browser,
                               const GURL& url,
                               int event_flags);

  static void OpenOrMoveSavedGroupToNewWindow(Browser* browser,
                                              const SavedTabGroup* saved_group,
                                              int event_flags);

  // Delete the `saved_group`.
  static void DeleteSavedTabGroup(Browser* browser,
                                  const SavedTabGroup* saved_group,
                                  int event_flags);

  // Pin the saved tab group if it's unpinned, or unpin the saved tab group if
  // it's pinned.
  static void ToggleGroupPinState(Browser* browser,
                                  base::Uuid id,
                                  int event_flags);

  // Create the the context menu model for a saved tab group button or a saved
  // tab group menu item in the Everything menu. `browser` is the one from
  // which this method is invoked. `saved_guid` is the saved tab group's Uuid.
  // Show a "Pin / Unpin group from bookmarks bar" option if
  // `show_pin_unpin_option` is true.
  static std::unique_ptr<ui::DialogModel> CreateSavedTabGroupContextMenuModel(
      Browser* browser,
      const base::Uuid& saved_guid,
      bool show_pin_unpin_option);

  // Converts a webcontents into a SavedTabGroupTab.
  static SavedTabGroupTab CreateSavedTabGroupTabFromWebContents(
      content::WebContents* contents,
      base::Uuid saved_tab_group_id);

  static content::NavigationHandle* OpenTabInBrowser(
      const GURL& url,
      Browser* browser,
      Profile* profile,
      WindowOpenDisposition disposition,
      std::optional<int> tabstrip_index = std::nullopt,
      std::optional<tab_groups::TabGroupId> local_group_id = std::nullopt);

  // Returns the Browser that contains a local group with id `group_id`.
  static Browser* GetBrowserWithTabGroupId(tab_groups::TabGroupId group_id);

  // Finds the TabGroup with id `group_id` across all Browsers.
  static TabGroup* GetTabGroupWithId(tab_groups::TabGroupId group_id);

  // Returns the list of WebContentses in the local group `group_id` in order.
  static std::vector<content::WebContents*> GetWebContentsesInGroup(
      tab_groups::TabGroupId group_id);

  // Returns the set of urls currently stored in the saved tab group.
  static std::unordered_set<std::string> GetURLsInSavedTabGroup(
      const tab_groups::SavedTabGroupKeyedService& saved_tab_group_service,
      const base::Uuid& saved_id);

  // Moves an open saved tab group from `source_browser` to `target_browser`.
  static void MoveGroupToExistingWindow(
      Browser* source_browser,
      Browser* target_browser,
      const tab_groups::TabGroupId& local_group_id,
      const base::Uuid& saved_group_id);

  // Activates the first tab in the saved group. If a tab in the group is
  // already activated, then we focus the window the group belongs to instead.
  static void FocusFirstTabOrWindowInOpenGroup(
      tab_groups::TabGroupId local_group_id);

  // Returns whether the tab's URL is viable for saving in a saved tab
  // group.
  static bool IsURLValidForSavedTabGroups(const GURL& gurl);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool IsTabGroupSavesUIUpdateMigrated(PrefService* pref_service);

  static void SetTabGroupSavesUIUpdateMigrated(PrefService* pref_service);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
