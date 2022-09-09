// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"

class Browser;
class SavedTabGroupModel;
class TabStripModel;

// Serves to maintain and listen to browsers who contain saved tab groups and
// update the model if a saved tab group was changed.
class SavedTabGroupModelListener : public BrowserListObserver,
                                   public TabStripModelObserver {
 public:
  // Used for testing.
  SavedTabGroupModelListener();
  explicit SavedTabGroupModelListener(SavedTabGroupModel* model);
  SavedTabGroupModelListener(const SavedTabGroupModelListener&) = delete;
  SavedTabGroupModelListener& operator=(
      const SavedTabGroupModelListener& other) = delete;
  ~SavedTabGroupModelListener() override;

  TabStripModel* GetTabStripModelWithTabGroupId(
      tab_groups::TabGroupId group_id);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;

 private:
  base::flat_set<raw_ptr<Browser>> observed_browsers_;
  raw_ptr<SavedTabGroupModel> model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_
