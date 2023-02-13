// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "content/public/browser/page.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
class SavedTabGroupButton;

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
  void SavedTabGroupAddedLocally(const base::GUID& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedLocally(
      const base::GUID& group_guid,
      const absl::optional<base::GUID>& tab_guid = absl::nullopt) override;
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupAddedFromSync(const base::GUID& guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::GUID& group_guid,
      const absl::optional<base::GUID>& tab_guid = absl::nullopt) override;

  // Calculates what the visible width would be when a restriction on width is
  // placed on the bar.
  int CalculatePreferredWidthRestrictedBy(int width_restriction);

 private:
  // Adds the saved group denoted by `guid` as a button in the
  // `SavedTabGroupBar` if the `guid` exists in `saved_tab_group_model_`.
  void SavedTabGroupAdded(const base::GUID& guid);

  // Removes the button denoted by `removed_group`'s guid from the
  // `SavedTabGroupBar`.
  void SavedTabGroupRemoved(const base::GUID& guid);

  // Updates the button (color, name, tab list) denoted by `guid` in the
  // `SavedTabGroupBar` if the `guid` exists in `saved_tab_group_model_`.
  void SavedTabGroupUpdated(const base::GUID& guid);

  // Adds the button to the child views for a new tab group at a specific index.
  // Also adds a button ptr to the tab_group_buttons_ list.
  void AddTabGroupButton(const SavedTabGroup& group, int index);

  // Adds all buttons currently stored in `saved_tab_group_model_`.
  void AddAllButtons();

  // Removes the button from the child views at a specific index. Also removes
  // the button ptr from the tab_group_buttons_ list.
  void RemoveTabGroupButton(const base::GUID& guid);

  // Remove all buttons currently in the bar.
  void RemoveAllButtons();

  // Find the button that matches `guid`.
  views::View* GetButton(const base::GUID& guid);

  // The callback that the button calls when clicked by a user.
  void OnTabGroupButtonPressed(const base::GUID& id, const ui::Event& event);

  // When called, display a bubble which shows all the groups that are saved
  // and not visible. Each entry in the bubble, when clicked, should open the
  // group into the tabstrip.
  void OnOverflowButtonPressed(views::View* bar, const ui::Event& event);

  // TODO: Move implementation inside of STGOverflowButton.
  void HideOverflowButton();
  void ShowOverflowButton();

  // Provides a callback that returns the page navigator
  base::RepeatingCallback<content::PageNavigator*()> GetPageNavigatorGetter();

  raw_ptr<views::MenuButton> overflow_button_;

  // Used to show the overflow menu when clicked.
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;

  // The model this tab group bar listens to.
  raw_ptr<SavedTabGroupModel> saved_tab_group_model_;

  // The page navigator used to create tab groups
  raw_ptr<content::PageNavigator, DanglingUntriaged> page_navigator_ = nullptr;
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
