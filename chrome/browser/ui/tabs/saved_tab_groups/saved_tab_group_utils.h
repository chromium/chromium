// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_

#include "base/uuid.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Browser;
class Profile;

class SavedTabGroupTab;
namespace content {
class WebContents;
}

class SavedTabGroupUtils {
 public:
  SavedTabGroupUtils() = delete;
  SavedTabGroupUtils(const SavedTabGroupUtils&) = delete;
  SavedTabGroupUtils& operator=(const SavedTabGroupUtils&) = delete;

  // Converts a webcontents into a SavedTabGroupTab.
  static SavedTabGroupTab CreateSavedTabGroupTabFromWebContents(
      content::WebContents* contents,
      base::Uuid saved_tab_group_id);

  static content::WebContents* OpenTabInBrowser(
      const GURL& url,
      Browser* browser,
      Profile* profile,
      WindowOpenDisposition disposition,
      absl::optional<int> tabstrip_index = absl::nullopt,
      absl::optional<tab_groups::TabGroupId> local_group_id = absl::nullopt);

  // Returns the Browser that contains a local group with id `group_id`.
  static Browser* GetBrowserWithTabGroupId(tab_groups::TabGroupId group_id);

  // Finds the TabGroup with id `group_id` across all Browsers.
  static TabGroup* GetTabGroupWithId(tab_groups::TabGroupId group_id);

  // Returns the list of WebContentses in the local group `group_id` in order.
  static std::vector<content::WebContents*> GetWebContentsesInGroup(
      tab_groups::TabGroupId group_id);

  // Returns whether the tab's URL is viable for saving in a saved tab group.
  static bool IsURLValidForSavedTabGroups(const GURL& gurl);
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
