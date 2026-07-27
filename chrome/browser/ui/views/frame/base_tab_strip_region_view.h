// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BASE_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BASE_TAB_STRIP_REGION_VIEW_H_

#include <optional>
#include <set>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tabs/public/tab_collection.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserView;
class HoverTabSelector;
class RootTabCollectionNode;
class TabDragContext;
class TabDragTarget;
class TabStripModel;
class TabStripCollectionController;
class TabStripView;
class PinnedTabContainerView;
class UnpinnedTabContainerView;
class TabDragHandler;
class TabCollectionNode;

// Shared collection-based foundation for tabstrip region views. Manages
// observers and controllers used for both horizontal and vertical orientations.
class BaseTabStripRegionView : public TabStripRegionView,
                               public views::WidgetObserver {
  METADATA_HEADER(BaseTabStripRegionView, TabStripRegionView)

 public:
  BaseTabStripRegionView(BrowserView* browser_view,
                         actions::ActionItem* root_action_item,
                         TabStripOrientation orientation);
  ~BaseTabStripRegionView() override;

  PinnedTabContainerView* GetPinnedTabsContainer();
  UnpinnedTabContainerView* GetUnpinnedTabsContainer();

  TabStripCollectionController* GetTabStripCollectionController() {
    return tab_strip_controller_.get();
  }

  TabDragTarget* GetTabDragTarget(const gfx::Point& point_in_screen) override;
  virtual gfx::Rect GetTabStripDraggableBounds() const = 0;

  // TabStripRegionView:
  void InitializeTabStrip() override;
  void ResetTabStrip() override;
  bool IsTabStripEditable() const override;
  bool IsTabStripCloseable() const override;
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) override;
  std::optional<int> GetFocusedTabIndex() const override;
  const tabs::TabData& GetTabData(const tabs::TabHandle& tab) override;
  views::View* GetTabAnchorViewAt(int tab_index) override;
  views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override {}
  TabDragContext* GetDragContext() override;
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
  void OnGlassFrameEligibilityChanged(bool is_eligible) override;
  bool TraverseUsingUpDownKeys() override;
  std::unique_ptr<ExpandOnHoverLock> GetExpandOnHoverLock(
      ExpandOnHoverLockType lock_type) override;

  // BrowserRootView::DropTarget:
  void HandleDragUpdate(
      const std::optional<BrowserRootView::DropIndex>& index) override;
  void HandleDragExited() override;

  // Views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  void DisableTabStripEditingForTesting() override;
  gfx::Rect GetLinkDropBoundsForTesting(
      const BrowserRootView::DropIndex& drop_index,
      DropArrow::Direction* direction);

  RootTabCollectionNode* root_node_for_testing() { return root_node_.get(); }
  TabStripOrientation orientation() const { return orientation_; }

 protected:
  virtual void OnTabStripViewSet() {}
  virtual void OnTabStripViewWillClear() {}

  void RecordNewTabButtonPressed();
  virtual void OnActiveTabChanged(const tabs::TabInterface* active_tab);

  void SetLinkDropArrow(const std::optional<BrowserRootView::DropIndex>& index);
  virtual gfx::Point GetLinkDropArrowPosition(
      const BrowserRootView::DropIndex& drop_index,
      DropArrow::Direction* direction) = 0;

  BrowserView* browser_view() const { return browser_view_; }
  actions::ActionItem* root_action_item() const { return root_action_item_; }
  TabStripModel* tab_strip_model() const { return tab_strip_model_; }
  TabStripView* tab_strip_view() const { return tab_strip_view_; }
  RootTabCollectionNode* root_node() const { return root_node_.get(); }
  DropArrow* drop_arrow() const { return drop_arrow_.get(); }
  TabHoverCardController* hover_card_controller() const {
    return hover_card_controller_.get();
  }

 private:
  views::View* SetTabStripView(std::unique_ptr<views::View> view);
  void ClearTabStripView(views::View* view);

  void OnChildrenAdded(const tabs::TabCollectionNodes& handles);
  void OnChildrenRemoved();
  void OnChildMoved(TabCollectionNode* moved_node);

  gfx::Rect GetLinkDropBounds(const BrowserRootView::DropIndex& drop_index,
                              DropArrow::Direction* direction);
  gfx::Rect GetLinkDropBoundsFromPosition(gfx::Point position,
                                          DropArrow::Direction direction);

  const TabStripOrientation orientation_;
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<actions::ActionItem> root_action_item_;
  const raw_ptr<TabStripModel> tab_strip_model_ = nullptr;

  bool tab_strip_editable_for_testing_ = true;

  raw_ptr<TabStripView> tab_strip_view_ = nullptr;
  raw_ptr<TabDragHandler> drag_handler_ = nullptr;
  std::unique_ptr<DropArrow> drop_arrow_;

  std::unique_ptr<TabStripCollectionController> tab_strip_controller_;
  std::unique_ptr<RootTabCollectionNode> root_node_;

  std::unique_ptr<TabHoverCardController> hover_card_controller_;
  std::unique_ptr<HoverTabSelector> hover_tab_selector_;

  std::optional<base::CallbackListSubscription> on_children_added_subscription_;
  std::optional<base::CallbackListSubscription>
      on_children_removed_subscription_;
  std::optional<base::CallbackListSubscription> on_child_moved_subscription_;
  std::optional<base::CallbackListSubscription>
      on_active_tab_changed_subscription_;

  std::optional<base::TimeTicks> new_tab_button_pressed_start_time_;

  bool is_first_window_presentation_ = true;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BASE_TAB_STRIP_REGION_VIEW_H_
