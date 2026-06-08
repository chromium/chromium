// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class TabCollectionNode;
class VerticalPinnedTabContainerView;
class VerticalUnpinnedTabContainerView;

namespace views {
class ScrollView;
class Separator;
}  // namespace views

// The view class for vertical tab strip which holds the pinned and unpinned
// regions and associates them to their scroll views. It also is responsible for
// scrolling to the active tab view when the active tab changes.
class VerticalTabStripView final : public views::View,
                                   public views::LayoutDelegate,
                                   public views::WidgetObserver {
  METADATA_HEADER(VerticalTabStripView, views::View)

 public:
  explicit VerticalTabStripView(TabCollectionNode* collection_node);
  VerticalTabStripView(const VerticalTabStripView&) = delete;
  VerticalTabStripView& operator=(const VerticalTabStripView&) = delete;
  ~VerticalTabStripView() override;

  views::Separator* GetTabsSeparator() { return tabs_separator_; }

  VerticalPinnedTabContainerView* GetPinnedTabsContainer();
  VerticalUnpinnedTabContainerView* GetUnpinnedTabsContainer();

  views::ScrollView* pinned_tabs_scroll_view() {
    return pinned_tabs_scroll_view_;
  }
  views::ScrollView* unpinned_tabs_scroll_view() {
    return unpinned_tabs_scroll_view_;
  }

  void SetCollapsedState(bool is_collapsed);
  void SetIsAnimatingSize(bool is_animating);

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  void OnActiveTabChanged(const tabs::TabInterface* active_tab);

  // Ensures that `view` is visible within its respective ScrollView.
  // Can be called to follow keyboard navigation and focus changes.
  void EnsureViewVisible(views::View* view);

  void RecordMousePressedInTab();
  bool IsFocusInTabStrip();

 private:
  class ActivatedViewTracker;

  views::View* AddScrollViewContents(std::unique_ptr<views::View> view);
  void RemoveScrollViewContents(views::View* view);
  void SetScrollViewProperties(views::ScrollView* scroll_view);
  void ResetCollectionNode();

  // Invoked after layout has been invoked for the activated view's associated
  // ScrollView. Ensures that the activated view is visible in the viewport.
  void EnsureVisibleInViewportPostActivationAndLayout(
      views::ScrollView* scroll_view);

  // Enables and disables overflow visuals on `scroll_view` respectively. Used
  // in combination to avoid visual artifacts caused by repeatedly scrolling-in
  // animating views.
  void EnableOverflowVisuals(views::ScrollView* scroll_view);
  void DisableOverflowVisuals(views::ScrollView* scroll_view);

  void UpdateColors();

  bool IsFrameActive() const;

  void HideHoverCardOnScroll();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;
  raw_ptr<views::ScrollView> pinned_tabs_scroll_view_ = nullptr;
  raw_ptr<VerticalPinnedTabContainerView> pinned_tabs_container_view_ = nullptr;
  raw_ptr<views::Separator> tabs_separator_ = nullptr;
  raw_ptr<views::ScrollView> unpinned_tabs_scroll_view_ = nullptr;
  raw_ptr<VerticalUnpinnedTabContainerView> unpinned_tabs_container_view_ =
      nullptr;
  bool is_collapsed_ = false;

  // Used for seek time metrics from the time the mouse enters the tabstrip.
  std::optional<base::TimeTicks> mouse_entered_tabstrip_time_;
  // Used to track if the time from mouse entered to tab switch been reported.
  bool has_reported_time_mouse_entered_to_switch_ = false;

  // Tracks the most recently activated view as reported by
  // `OnActiveTabChanged()`.
  std::unique_ptr<ActivatedViewTracker> activated_view_tracker_;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;
  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  bool is_first_window_presentation_ = true;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
