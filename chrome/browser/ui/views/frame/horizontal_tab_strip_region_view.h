// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"

class BrowserView;

namespace views {
class ActionViewController;
class Button;
}
class NewTabButton;
class TabStripActionContainer;
class TabStripComboButton;
class TabStrip;
class TabStripScrollContainer;
class TabStripControlButton;
class TabScrollButtonContainer;

// Container for the tabstrip and the other views sharing space with it -
// with the exception of the caption buttons.
class HorizontalTabStripRegionViewOld : public TabStripRegionView {
  METADATA_HEADER(HorizontalTabStripRegionViewOld, TabStripRegionView)

 public:
  explicit HorizontalTabStripRegionViewOld(BrowserView* browser_view);
  HorizontalTabStripRegionViewOld(const HorizontalTabStripRegionViewOld&) =
      delete;
  HorizontalTabStripRegionViewOld& operator=(
      const HorizontalTabStripRegionViewOld&) = delete;
  ~HorizontalTabStripRegionViewOld() override;

  // Returns true if |point| falls within the window caption area of the
  // horizontal tab strip. Returns false if the point hits an interactive child
  // view. |point| is in the local coordinate space of |this|.
  bool IsPositionInWindowCaption(const gfx::Point& point) override;

  // views::View:
  // The TabSearchButton and NewTabButton may need to be rendered above the
  // TabStrip, but FlexLayout needs the children to be stored in the correct
  // order in the view.
  views::View::Views GetChildrenInZOrder() override;

  // Calls the parent Layout, but in some cases may also need to manually
  // position the TabSearchButton to layer over the TabStrip.
  void Layout(PassKey) override;

  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;
  views::View* GetDefaultFocusableChild() override;

  Profile* profile();

  TabStrip* tab_strip() { return tab_strip_; }

  // TabStripRegionView:
  void InitializeTabStrip() override;
  void ResetTabStrip() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool IsTabStripEditable() const override;
  void DisableTabStripEditingForTesting() override;
  bool IsTabStripCloseable() const override;
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) override;
  std::optional<int> GetFocusedTabIndex() const override;
  const tabs::TabData& GetTabData(const tabs::TabHandle& tab) override;
  views::View* GetTabAnchorView(const tabs::TabHandle& tab) override;
  views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;
  TabDragContext* GetDragContext() override;
  TabDragTarget* GetTabDragTarget(const gfx::Point& point_in_screen) override;
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;
  bool CanDrop(const OSExchangeData& data) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  void SetTabStripObserver(TabStripObserver* observer) override;
  views::View* GetTabStripView() override;
  TabHoverCardController* GetHoverCardController() override;
  std::unique_ptr<ExpandOnHoverLock> GetExpandOnHoverLock(
      ExpandOnHoverLockType lock_type) override;
  void OnGlassFrameEligibilityChanged(bool is_eligible) override;

  bool HasLeadingButtons() const;

 private:
  // Updates the border padding for `new_tab_button_`.  This should be called
  // whenever any input of the computation of the border's sizing changes.
  void UpdateButtonBorders();

  // Updates the left and right margins for the tab strip.
  void UpdateTabStripMargin();

  // Gets called on `Layout` and adjusts the x-axis position of the `view` based
  // on `offset`. This should only used for views that show before tab strip.
  void AdjustViewBoundsRect(View* view, int offset);

  bool tab_strip_set_ = false;

  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<TabStripActionContainer> tab_strip_action_container_ = nullptr;
  raw_ptr<views::View> tab_strip_container_ = nullptr;
  raw_ptr<views::View> reserved_grab_handle_space_ = nullptr;
  raw_ptr<TabStrip> tab_strip_ = nullptr;
  raw_ptr<TabStripScrollContainer> tab_strip_scroll_container_ = nullptr;
  raw_ptr<TabStripComboButton> combo_button_ = nullptr;
  raw_ptr<views::Button> new_tab_button_ = nullptr;
  raw_ptr<TabStripControlButton> unfocus_button_ = nullptr;

  std::unique_ptr<views::ActionViewController> action_view_controller_;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(base::BindRepeating(
          &HorizontalTabStripRegionViewOld::UpdateButtonBorders,
          base::Unretained(this)));
};

class HorizontalTabStripRegionViewNew : public BaseTabStripRegionView {
  METADATA_HEADER(HorizontalTabStripRegionViewNew, BaseTabStripRegionView)

 public:
  explicit HorizontalTabStripRegionViewNew(BrowserView* browser_view);
  HorizontalTabStripRegionViewNew(const HorizontalTabStripRegionViewNew&) =
      delete;
  HorizontalTabStripRegionViewNew& operator=(
      const HorizontalTabStripRegionViewNew&) = delete;
  ~HorizontalTabStripRegionViewNew() override;

  bool IsPositionInWindowCaption(const gfx::Point& point) override;
  views::View::Views GetChildrenInZOrder() override;
  void Layout(PassKey) override;

  bool HasLeadingButtons() const { return false; }

  // TabStripRegionView:
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::View* GetTabStripView() override;
  gfx::Rect GetTabStripDraggableBounds() const override;
  gfx::Point GetLinkDropArrowPosition(
      const BrowserRootView::DropIndex& drop_index,
      DropArrow::Direction* direction) override;

  TabScrollButtonContainer* scroll_button_container_for_testing() {
    return scroll_button_container_;
  }

 private:
  void OnTabStripViewSet() override;
  void OnTabStripViewWillClear() override;
  // Computes if the unpinned container would be scrollable
  // if we did not show the scroll buttons. To be used only in Layout().
  bool ComputeIsUnpinnedTabsScrollable(views::ManualLayoutUtil& layout_util);

  void UpdateButtonBorders();

  raw_ptr<TabStripActionContainer> tab_strip_action_container_ = nullptr;
  raw_ptr<views::View> reserved_grab_handle_space_ = nullptr;
  raw_ptr<TabStripComboButton> combo_button_ = nullptr;
  raw_ptr<views::Button> new_tab_button_ = nullptr;
  raw_ptr<TabScrollButtonContainer> scroll_button_container_ = nullptr;

  std::unique_ptr<views::ActionViewController> action_view_controller_;

  base::CallbackListSubscription subscription_;
};

using HorizontalTabStripRegionView = HorizontalTabStripRegionViewOld;

std::unique_ptr<TabStripRegionView> CreateHorizontalTabStripRegionView(
    BrowserView* browser_view);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_
