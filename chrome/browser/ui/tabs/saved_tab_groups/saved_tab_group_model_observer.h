// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"

// Serves to notify any SavedTabGroupModel listeners that a change has occurred
// supply the SavedTabGroup that was changed.
class SavedTabGroupModelObserver {
 public:
  SavedTabGroupModelObserver(const SavedTabGroupModelObserver&) = delete;
  SavedTabGroupModelObserver& operator=(const SavedTabGroupModelObserver&) =
      delete;

  // Called when a saved tab group is added to the backend.
  virtual void SavedTabGroupAdded(const SavedTabGroup& group, int index) {}

  // Called when a saved tab group will be removed from the backend.
  virtual void SavedTabGroupRemoved(int index) {}

  // Called when the title, urls, or color change
  virtual void SavedTabGroupUpdated(const SavedTabGroup& group, int index) {}

  // Called when the order of saved tab groups in the bookmark bar are changed
  virtual void SavedTabGroupMoved(const SavedTabGroup& group,
                                  int old_index,
                                  int new_index) {}

 protected:
  SavedTabGroupModelObserver() = default;
  virtual ~SavedTabGroupModelObserver() = default;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
