// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_observer.h"
#include "content/public/browser/page.h"
#include "ui/views/accessible_pane_view.h"

class Browser;

namespace content {
class PageNavigator;
}

// The view for accessing SavedTabGroups from the bookmarks bar. Is responsible
// for rendering the SavedTabGroupButtons with the bounds that are defined by
// its parent, BookmarkBarView.
class SavedTabGroupBar : public views::AccessiblePaneView,
                         public SavedTabGroupModelObserver {
 public:
  SavedTabGroupBar(Browser* browser, bool animations_enabled);
  SavedTabGroupBar(Browser* browser,
                   SavedTabGroupModel* saved_tab_group_model,
                   bool animations_enabled);
  SavedTabGroupBar(const SavedTabGroupBar&) = delete;
  SavedTabGroupBar& operator=(const SavedTabGroupBar&) = delete;
  ~SavedTabGroupBar() override;

  // Sets the stored page navigator
  void SetPageNavigator(content::PageNavigator* page_navigator) {
    page_navigator_ = page_navigator;
  }

  content::PageNavigator* page_navigator() { return page_navigator_; }

  // views::AccessiblePaneView
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAdded(const SavedTabGroup& group, int index) override;
  void SavedTabGroupRemoved(int index) override;
  void SavedTabGroupUpdated(const SavedTabGroup& group, int index) override;
  void SavedTabGroupMoved(const SavedTabGroup& group,
                          int old_index,
                          int new_index) override;

  // Calculates what the visible width would be when a restriction on width is
  // placed on the bar.
  int CalculatePreferredWidthRestrictedBy(int width_restriction);

 private:
  // Adds the button to the child views for a new tab group at a specific index.
  // Also adds a button ptr to the tab_group_buttons_ list.
  void AddTabGroupButton(const SavedTabGroup& group, int index);

  // Removes the button from the child views at a specific index. Also removes
  // the button ptr from the tab_group_buttons_ list.
  void RemoveTabGroupButton(int index);

  // Remove all buttons currently in the bar.
  void RemoveAllButtons();

  // the callback that the button calls when clicked by a user.
  void OnTabGroupButtonPressed(const base::GUID& id, const ui::Event& event);

  // Provides a callback that returns the page navigator
  base::RepeatingCallback<content::PageNavigator*()> GetPageNavigatorGetter();

  // the model this tab group bar listens to.
  raw_ptr<SavedTabGroupModel> saved_tab_group_model_;

  // the page navigator used to create tab groups
  raw_ptr<content::PageNavigator> page_navigator_ = nullptr;

  raw_ptr<Browser> browser_;

  // animations have been noted to cause issues with tests in the bookmarks bar.
  // this boolean lets the SavedTabGroupButton choose whether they want to
  // animate or not.
  const bool animations_enabled_ = true;

  // Returns WeakPtrs used in GetPageNavigatorGetter(). Used to ensure
  // safety if BookmarkBarView is deleted after getting the callback.
  base::WeakPtrFactory<SavedTabGroupBar> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
