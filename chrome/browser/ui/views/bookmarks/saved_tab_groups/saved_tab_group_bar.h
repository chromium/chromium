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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class SavedTabGroupButton;
class SavedTabGroupDragData;

namespace content {
class PageNavigator;
}

namespace views {
class Widget;
}

// The view for accessing SavedTabGroups from the bookmarks bar. Is responsible
// for rendering the SavedTabGroupButtons with the bounds that are defined by
// its parent, BookmarkBarView.
class SavedTabGroupBar : public views::AccessiblePaneView,
                         public SavedTabGroupModelObserver,
                         public views::WidgetObserver {
 public:
  METADATA_HEADER(SavedTabGroupBar);
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

  // views::View
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) override;
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupReorderedFromSync() override;
  void SavedTabGroupTabsReorderedLocally(const base::Uuid& group_guid) override;
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) override;

  // WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;

  // Calculates what the visible width would be when a restriction on width is
  // placed on the bar.
  int CalculatePreferredWidthRestrictedBy(int width_restriction) const;

  bool IsOverflowButtonVisible();

 private:
  // Overrides the View methods needed to be a drop target for saved tab groups.
  class OverflowMenu;

  // Adds the saved group denoted by `guid` as a button in the
  // `SavedTabGroupBar` if the `guid` exists in `saved_tab_group_model_`.
  void SavedTabGroupAdded(const base::Uuid& guid);

  // Removes the button denoted by `removed_group`'s guid from the
  // `SavedTabGroupBar`.
  void SavedTabGroupRemoved(const base::Uuid& guid);

  // Updates the button (color, name, tab list) denoted by `guid` in the
  // `SavedTabGroupBar` if the `guid` exists in `saved_tab_group_model_`.
  void SavedTabGroupUpdated(const base::Uuid& guid);

  // Reorders all groups in the bookmarks to match the state of
  // `saved_tab_group_model_`.
  void SavedTabGroupReordered();

  // Adds the button to the child views for a new tab group at a specific index.
  // This function then verifies if the added button and overflow button should
  // be visible/hidden. Also adds a button ptr to the tab_group_buttons_ list.
  void AddTabGroupButton(const SavedTabGroup& group, int index);

  // Adds all buttons currently stored in `saved_tab_group_model_` using
  // SavedTabGroupBar::AddTabGroupButton.
  void LoadAllButtonsFromModel();

  // Removes the button from the child views at a specific index. Also removes
  // the button ptr from the tab_group_buttons_ list.
  void RemoveTabGroupButton(const base::Uuid& guid);

  // Remove all buttons currently in the bar.
  void RemoveAllButtons();

  // Find the button that matches `guid`.
  views::View* GetButton(const base::Uuid& guid);

  // The callback that the button calls when clicked by a user.
  void OnTabGroupButtonPressed(const base::Uuid& id, const ui::Event& event);

  // When called, display a bubble which shows all the groups that are saved
  // and not visible. Each entry in the bubble, when clicked, should open the
  // group into the tabstrip.
  void MaybeShowOverflowMenu();

  // Updates the contents of the overflow menu if it is open.
  void UpdateOverflowMenu();

  // TODO: Move implementation inside of STGOverflowButton.
  void HideOverflowButton();
  void ShowOverflowButton();

  // Returns the number of currently visible groups. Does not include the
  // overflow button or button housed in its view.
  int GetNumberOfVisibleGroups() const;

  // Updates the visibilites of all buttons up to `last_index_visible`. The
  // overflow button will be displayed based on `should_show_overflow`.
  void UpdateButtonVisibilities(bool should_show_overflow,
                                size_t last_visible_button_index);

  // Returns true if we should show the overflow button because there is not
  // enough space to display all the buttons or if there are more buttons than
  // the maximum visible.
  bool ShouldShowOverflowButtonForWidth(int max_width) const;

  // Finds the index of the last button that can be displayed within the given
  // width. Guaranteed to not exceed `kMaxVisibleButtons`. Does not include the
  // overflow button.
  int CalculateLastVisibleButtonIndexForWidth(int max_width) const;

  // Updates the drop index in `drag_data_` based on the current drag location.
  void UpdateDropIndex();

  // Returns the drop index for the current drag session, if any.
  absl::optional<size_t> GetDropIndex() const;

  // Reorders the dragged group to its new index.
  void HandleDrop();

  // Paints the drop indicator, if one should be shown.
  void MaybePaintDropIndicatorInBar(gfx::Canvas* canvas);

  // Calculates the index in the saved tab groups bar at which we should show a
  // drop indicator, or nullopt if we should not show an indicator in the bar.
  absl::optional<int> CalculateDropIndicatorIndexInBar() const;

  // Calculates the index (in saved tab group model space, so across the bar and
  // the overflow menu) at which we should show a drop indicator, or nullopt if
  // we should not show an indicator anywhere at all.
  absl::optional<int> CalculateDropIndicatorIndexInCombinedSpace() const;

  // Provides a callback that returns the page navigator
  base::RepeatingCallback<content::PageNavigator*()> GetPageNavigatorGetter();

  raw_ptr<views::MenuButton, AcrossTasksDanglingUntriaged> overflow_button_;

  // Used to show the overflow menu when clicked.
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;

  // The model this tab group bar listens to.
  raw_ptr<SavedTabGroupModel> saved_tab_group_model_;

  // The page navigator used to create tab groups
  raw_ptr<content::PageNavigator, AcrossTasksDanglingUntriaged>
      page_navigator_ = nullptr;
  raw_ptr<Browser> browser_;

  // During a drag and drop session, `drag_data_` owns the state for the drag.
  std::unique_ptr<SavedTabGroupDragData> drag_data_;

  // The currently open overflow menu, or nullptr if one is not open now.
  raw_ptr<OverflowMenu> overflow_menu_ = nullptr;

  base::ScopedObservation<views::Widget, SavedTabGroupBar> widget_observation_{
      this};

  // animations have been noted to cause issues with tests in the bookmarks bar.
  // this boolean lets the SavedTabGroupButton choose whether they want to
  // animate or not.
  const bool animations_enabled_ = true;

  // Returns WeakPtrs used in GetPageNavigatorGetter(). Used to ensure
  // safety if BookmarkBarView is deleted after getting the callback.
  base::WeakPtrFactory<SavedTabGroupBar> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
