// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "content/public/browser/page.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace content {
class PageNavigator;
}

namespace views {
class Widget;
}

namespace tab_groups {

class TabGroupSyncService;
class SavedTabGroupButton;
class SavedTabGroupDragData;
class SavedTabGroupOverflowButton;

// The view for accessing SavedTabGroups from the bookmarks bar. Is responsible
// for rendering the SavedTabGroupButtons with the bounds that are defined by
// its parent, BookmarkBarView.
class SavedTabGroupBar : public views::AccessiblePaneView,
                         public SavedTabGroupModelObserver,
                         public TabGroupSyncService::Observer,
                         public views::WidgetObserver {
  METADATA_HEADER(SavedTabGroupBar, views::AccessiblePaneView)

 public:
  SavedTabGroupBar(Browser* browser, bool animations_enabled);
  SavedTabGroupBar(Browser* browser,
                   TabGroupSyncService* tab_group_service,
                   bool animations_enabled);
  SavedTabGroupBar(const SavedTabGroupBar&) = delete;
  SavedTabGroupBar& operator=(const SavedTabGroupBar&) = delete;
  ~SavedTabGroupBar() override;

  // Sets the stored page navigator.
  // TODO(pengchaocai): Navigator seems not needed. Investigate and remove.
  void SetPageNavigator(content::PageNavigator* page_navigator) {
    page_navigator_ = page_navigator;
  }

  content::PageNavigator* page_navigator() { return page_navigator_; }
  views::View* overflow_button() { return overflow_button_; }

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
  void Layout(PassKey) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group) override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupReorderedFromSync() override;
  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override;
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;

  // TabGroupSyncService::Observer
  void OnInitialized() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupLocalIdChanged(
      const base::Uuid& sync_id,
      const std::optional<LocalTabGroupID>& local_id) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override;
  void OnTabGroupsReordered(TriggerSource source) override;

  // WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;

  // Calculates what the visible width would be when a restriction on width is
  // placed on the bar.
  int CalculatePreferredWidthRestrictedBy(int width_restriction) const;

  // Calculates what the visible width would be when a restriction on width is
  // placed on the bar. Should only get invoked behind TabGroupsSaveV2.
  // TODO(crbug.com/329659664): Rename once V2 ships.
  int V2CalculatePreferredWidthRestrictedBy(int width_restriction) const;

  bool IsOverflowButtonVisible();

  // Returns the number of currently visible groups. Does not include the
  // overflow button or button housed in its view.
  int GetNumberOfVisibleGroups() const;

 private:
  // Overrides the View methods needed to be a drop target for saved tab groups.
  class OverflowMenu;

  // Adds the saved group denoted by `guid` as a button in the
  // `SavedTabGroupBar` if the `guid` exists in `saved_tab_group_model_`.
  void SavedTabGroupAdded(const base::Uuid& guid);

  // Removes the button denoted by `removed_group`'s guid from the
  // `SavedTabGroupBar`.
  void SavedTabGroupRemoved(const base::Uuid& guid);

  // Updates (adds/updates/removes) the button denoted by `guid` when calling
  // add/update methods.
  void UpsertSavedTabGroupButton(const base::Uuid& guid);

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

  // Returns the index of the group.
  std::optional<size_t> GetIndexOfGroup(const base::Uuid& guid) const;

  // The callback that the button calls when clicked by a user.
  void OnTabGroupButtonPressed(const base::Uuid& id, const ui::Event& event);

  // Creates the overflow button that houses saved tab groups that are not
  // visible in the SavedTabGroupBar.
  std::unique_ptr<SavedTabGroupOverflowButton> CreateOverflowButton();

  // When called, display a bubble which shows all the groups that are saved
  // and not visible. Each entry in the bubble, when clicked, should open the
  // group into the tabstrip.
  void MaybeShowOverflowMenu();

  // When called, display a menu that shows a "Create new tab group" option and
  // all the saved tab groups (if there are any). Pressing on the saved tab
  // groups opens the group into the tab strip.
  void ShowEverythingMenu();

  // Updates the contents of the overflow menu if it is open.
  void UpdateOverflowMenu();

  // TODO: Move implementation inside of STGOverflowButton.
  void HideOverflowButton();
  void ShowOverflowButton();

  // Updates the visibilites of all buttons up to `last_index_visible`. The
  // overflow button will be displayed based on `should_show_overflow`.
  void UpdateButtonVisibilities(bool should_show_overflow,
                                int last_visible_button_index);

  // Returns true if we should show the overflow button because there is not
  // enough space to display all the buttons or if there are more buttons than
  // the maximum visible.
  bool ShouldShowOverflowButtonForWidth(int max_width) const;

  // Finds the index of the last button that can be displayed within the given
  // width. Guaranteed to not exceed `kMaxVisibleButtons`. Does not include the
  // overflow button. Returns -1 to indicate that no tab groups button is
  // visible with the given width.
  int CalculateLastVisibleButtonIndexForWidth(int max_width) const;

  // Updates the drop index in `drag_data_` based on the current drag location.
  void UpdateDropIndex();

  // Returns the drop index for the current drag session, if any.
  std::optional<size_t> GetDropIndex() const;

  // Reorders the dragged group to its new index.
  void HandleDrop();

  // Paints the drop indicator, if one should be shown.
  void MaybePaintDropIndicatorInBar(gfx::Canvas* canvas);

  // Maybe show the promo if a group was closed from the tabstrip.
  void MaybeShowClosePromo(const base::Uuid& saved_group_id);

  // Calculates the index in the saved tab groups bar at which we should show a
  // drop indicator, or nullopt if we should not show an indicator in the bar.
  std::optional<int> CalculateDropIndicatorIndexInBar() const;

  // Calculates the index (in saved tab group model space, so across the bar and
  // the overflow menu) at which we should show a drop indicator, or nullopt if
  // we should not show an indicator anywhere at all.
  std::optional<int> CalculateDropIndicatorIndexInCombinedSpace() const;

  // Provides a callback that returns the page navigator
  base::RepeatingCallback<content::PageNavigator*()> GetPageNavigatorGetter();

  raw_ptr<views::MenuButton, AcrossTasksDanglingUntriaged> overflow_button_;

  std::unique_ptr<STGEverythingMenu> everything_menu_;

  // Used to show the overflow menu when clicked.
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;

  // The service used to manage and query SavedTabGroups.
  raw_ptr<TabGroupSyncService> tab_group_service_ = nullptr;

  // The page navigator used to create tab groups
  raw_ptr<content::PageNavigator, AcrossTasksDanglingUntriaged>
      page_navigator_ = nullptr;

  raw_ptr<Browser> browser_ = nullptr;

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

  // Determines if we should use the updated SavedTabGroups UI.
  const bool ui_update_enabled_;

  // Returns WeakPtrs used in GetPageNavigatorGetter(). Used to ensure
  // safety if BookmarkBarView is deleted after getting the callback.
  base::WeakPtrFactory<SavedTabGroupBar> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
