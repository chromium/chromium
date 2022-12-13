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
#include "content/public/browser/web_contents.h"

class SavedTabGroupModel;
class TabStripModel;
class Profile;

// Manages the listening state for each individual tabstrip.
class SavedTabGroupBrowserListener : public TabStripModelObserver {
 public:
  SavedTabGroupBrowserListener(Browser* browser, SavedTabGroupModel* model);
  ~SavedTabGroupBrowserListener() override;

  bool ContainsTabGroup(tab_groups::TabGroupId group_id) const;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(absl::optional<tab_groups::TabGroupId> group,
                              content::WebContents* contents,
                              int index) override;

  Browser* browser() { return browser_; }
  SavedTabGroupModel* saved_tab_group_model() { return model_; }

 private:
  std::unordered_map<content::WebContents*, base::Token>
      web_contents_to_tab_id_map_;
  raw_ptr<Browser> browser_;
  raw_ptr<SavedTabGroupModel> model_;
};

// Serves to maintain and listen to browsers who contain saved tab groups and
// update the model if a saved tab group was changed.
class SavedTabGroupModelListener : public BrowserListObserver {
 public:
  // Used for testing.
  SavedTabGroupModelListener();
  explicit SavedTabGroupModelListener(SavedTabGroupModel* model,
                                      Profile* profile);
  SavedTabGroupModelListener(const SavedTabGroupModelListener&) = delete;
  SavedTabGroupModelListener& operator=(
      const SavedTabGroupModelListener& other) = delete;
  ~SavedTabGroupModelListener() override;

  TabStripModel* GetTabStripModelWithTabGroupId(
      tab_groups::TabGroupId group_id);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Testing Accessors.
  std::unordered_map<Browser*, SavedTabGroupBrowserListener>&
  GetBrowserListenerMapForTesting() {
    return observed_browser_listeners_;
  }

 private:
  std::unordered_map<Browser*, SavedTabGroupBrowserListener>
      observed_browser_listeners_;
  raw_ptr<SavedTabGroupModel> model_ = nullptr;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_LISTENER_H_
