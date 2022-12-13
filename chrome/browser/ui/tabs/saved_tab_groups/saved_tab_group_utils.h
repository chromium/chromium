// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_

#include "base/guid.h"

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
      base::GUID saved_tab_group_id);
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_UTILS_H_
