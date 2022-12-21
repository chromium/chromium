// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_

#include "base/guid.h"

class Browser;

// The API for performing updates to the SavedTabGroup feature.
class SavedTabGroupController {
  // Opens a Saved Tab Group in a specified browser and sets all of the required
  // state in the SavedTabGroupService.
  virtual void OpenSavedTabGroupInBrowser(
      Browser* browser,
      const base::GUID& saved_group_guid) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_CONTROLLER_H_
