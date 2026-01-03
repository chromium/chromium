// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/resize_area_delegate.h"

class BrowserWindowInterface;
class RootTabCollectionNode;
class VerticalUnpinnedTabContainerView;
class VerticalPinnedTabContainerView;
class VerticalTabStripBottomContainer;
class VerticalTabStripTopContainer;
class TabDragContext;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace views {
class ResizeArea;
class Separator;
class View;
class FlexLayout;
}  // namespace views

// Container for the vertical tabstrip and the other views sharing space with
// it, excluding the caption buttons.
class VerticalTabStripRegionView final : public TabStripRegionView,
                                         public views::ResizeAreaDelegate,
                                         public gfx::AnimationDelegate {
  METADATA_HEADER(VerticalTabStripRegionView, views::AccessiblePaneView)

 public:
  static constexpr int kResizeAreaWidth = 6;
  // TODO(crbug.com/465833741): Replace constant with derived value based on
  // caption buttons.
  static constexpr int kUncollapsedMinWidth = 128;
  // TODO(crbug.com/465832180): Replace constant based width final max width for
  // view.
  static constexpr int kUncollapsedMaxWidth = 400;
  static constexpr int kCollapsedWidth = 48;
  // TODO(crbug.com/465833741): Determine snapping behavior.
  static constexpr int kCollapseSnapWidth =
      (kUncollapsedMinWidth + kCollapsedWidth) / 2;

  explicit VerticalTabStripRegionView(
      tabs::VerticalTabStripStateController* state_controller,
      actions::ActionItem* root_action_item,
      BrowserWindowInterface* browser);
  VerticalTabStripRegionView(const VerticalTabStripRegionView&) = delete;
  VerticalTabStripRegionView& operator=(const VerticalTabStripRegionView&) =
      delete;
  ~VerticalTabStripRegionView() override;

  views::Separator* tabs_separator_for_testing() {
    return tab_strip_view_->tabs_separator_for_testing();
  }
  views::ResizeArea* resize_area_for_testing() { return resize_area_; }
  VerticalPinnedTabContainerView* GetPinnedTabsContainer();
  VerticalUnpinnedTabContainerView* GetUnpinnedTabsContainer();

  RootTabCollectionNode* root_node_for_testing() { return root_node_.get(); }

  tabs::VerticalTabStripState target_collapse_state_for_testing() {
    return target_collapse_state_;
  }

  bool is_animating_for_testing() { return resize_animation_.is_animating(); }

  VerticalTabStripTopContainer* GetTopContainer() {
    return top_button_container_;
  }

  VerticalTabStripBottomContainer* GetBottomContainer() {
    return bottom_button_container_;
  }

  VerticalTabStripController* GetVerticalTabStripController() {
    return tab_strip_controller_.get();
  }

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;
  views::View* GetDefaultFocusableChild() override;

  // TabStripRegionView
  bool IsTabStripEditable() const override;
  void DisableTabStripEditingForTesting() const override;
  bool IsTabStripCloseable() const override;
  bool IsAnimating() const override;
  void StopAnimating() override;
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) override;
  std::optional<int> GetFocusedTabIndex() const override;
  views::View* GetTabAnchorViewAt(int tab_index) override;
  views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) override;
  TabDragContext* GetDragContext() override;
  void SetTabStripObserver(TabStripObserver* observer) override;

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  void CreateTabStripController(BrowserView* browser_view);

  // These methods provide the toolbar height and exclusion width, before the
  // layout of this view, for use in calculating positioning of child views. If
  // an exclusion width is provided, nothing can be rendered within the
  // rectangle defined by `(exclusion_width, toolbar_height)` that is aligned to
  // the leading, top corner.
  void SetToolbarHeightForLayout(const int toolbar_height);
  void SetExclusionWidthForLayout(const int exclusion_width);

  TabDragTarget* GetTabDragTarget(const gfx::Point& point_in_screen);

 private:
  views::View* SetTabStripView(std::unique_ptr<views::View> view);

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);
  void UpdateCollapseState(tabs::VerticalTabStripState new_state);
  void ResizeToWidth(int width);

  void UpdateBackgroundColors();

  bool IsFrameActive() const;

  // When false simulates a non-editable tabstrip. For testing only.
  bool tab_strip_editable_for_testing_ = true;

  raw_ptr<VerticalTabStripTopContainer> top_button_container_ = nullptr;
  raw_ptr<views::Separator> top_button_separator_ = nullptr;
  raw_ptr<VerticalTabStripView> tab_strip_view_ = nullptr;
  raw_ptr<VerticalTabStripBottomContainer> bottom_button_container_ = nullptr;
  raw_ptr<views::View> gemini_button_ = nullptr;
  raw_ptr<views::ResizeArea> resize_area_ = nullptr;
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;

  // The drag handler is a view (required for capturing mouse inputs during
  // a drag loop) owned by the tab strip's View.
  raw_ptr<TabDragContext> drag_handler_ = nullptr;

  std::unique_ptr<VerticalTabStripController> tab_strip_controller_;
  std::unique_ptr<RootTabCollectionNode> root_node_;

  const raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
  const raw_ptr<tabs::VerticalTabStripStateController> state_controller_;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;

  // The width of the vertical tabstrip at the beginning of the current resize
  // operation. Is std::nullopt when not resizing.
  std::optional<int> starting_width_on_resize_ = std::nullopt;

  // The intended collapse state by the user as a result of dragging the resize
  // area. This differs from the state controller in that its uncollapsed_width
  // updates throughout a drag operation, whereas the state controller only
  // updates its uncollapsed width when a drag-to-uncollapse operation ends.
  // Additionally, the collapsed value may differ from the state controller, in
  // which case this is the source of truth only if we are in a drag operation.
  tabs::VerticalTabStripState target_collapse_state_;

  // Animation for collapsing (GetCurrentValue() -> 0) and expanding
  // (GetCurrentValue() -> 1).
  gfx::SlideAnimation resize_animation_;

  // The width of the exclusion zone. This is used to determine when to toggle
  // the collapse state of the state controller.
  std::optional<int> exclusion_width_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_VERTICAL_TAB_STRIP_REGION_VIEW_H_
