// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "components/tabs/public/tab_group.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

class VerticalTabStripView::ActivatedViewTracker : public views::ViewObserver {
 public:
  ActivatedViewTracker() = default;
  ActivatedViewTracker(const ActivatedViewTracker&) = delete;
  ActivatedViewTracker& operator=(const ActivatedViewTracker&) = delete;
  ~ActivatedViewTracker() override = default;

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    SetView(nullptr);
  }
  void OnViewRemovedFromWidget(views::View* observed_view) override {
    SetView(nullptr);
  }
  void OnViewBoundsChanged(views::View* observed_view) override {
    CheckTrackedViewHeight();
  }
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    CheckTrackedViewHeight();
  }

  void SetView(views::View* view) {
    if (view == view_) {
      return;
    }
    observation_.Reset();
    on_reached_preferred_height_cb_.Reset();
    view_ = view;

    if (view_) {
      observation_.Observe(view_.get());
    }
  }
  views::View* view() { return view_; }

  // Returns true if the tracked view height matches its preferred height.
  bool IsViewAtPreferredHeight() {
    return view_->size().height() == view_->GetPreferredSize().height();
  }

  // Sets a callback that is run when the tracked view's height reaches its
  // preferred height.
  void SetOnReachedPreferredHeightCallback(
      base::OnceClosure on_reached_preferred_height_cb) {
    on_reached_preferred_height_cb_ = std::move(on_reached_preferred_height_cb);
    CheckTrackedViewHeight();
  }

 private:
  void CheckTrackedViewHeight() {
    CHECK(view_);
    if (IsViewAtPreferredHeight() && on_reached_preferred_height_cb_) {
      std::move(on_reached_preferred_height_cb_).Run();
    }
  }

  raw_ptr<views::View> view_ = nullptr;
  base::OnceClosure on_reached_preferred_height_cb_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

VerticalTabStripView::VerticalTabStripView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      activated_view_tracker_(std::make_unique<ActivatedViewTracker>()) {
  // Paint to a layer and mask to bounds to prevent tabs from overflowing and
  // drawing outside the window boundaries on Linux when the window is small.
  // This is configured here rather than a higher-level container so that drop
  // shadows are not clipped.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetProperty(views::kElementIdentifierKey, kTabStripElementId);

  pinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(pinned_tabs_scroll_view_);

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  unpinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(unpinned_tabs_scroll_view_);

  collection_node->set_add_child_to_node(base::BindRepeating(
      &VerticalTabStripView::AddScrollViewContents, base::Unretained(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &VerticalTabStripView::RemoveScrollViewContents, base::Unretained(this)));

  callback_subscriptions_.emplace_back(
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabStripView::ResetCollectionNode, base::Unretained(this))));

  SetNotifyEnterExitOnChild(true);
  UpdateColors();
}

VerticalTabStripView::~VerticalTabStripView() = default;

views::ProposedLayout VerticalTabStripView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.width().is_bounded()) {
    return layouts;
  }

  const int region_horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  const int region_vertical_padding = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);

  int y = 0;

  // Determine container preferred heights.
  views::SizeBounds pinned_tab_container_size_bounds =
      size_bounds.Inset(gfx::Insets::TLBR(0, region_horizontal_padding, 0, 0));
  const int pinned_preferred_height =
      pinned_tabs_scroll_view_
          ->GetPreferredSize(pinned_tab_container_size_bounds)
          .height();
  const int unpinned_preferred_height =
      unpinned_tabs_scroll_view_->GetPreferredSize(size_bounds).height();

  const bool should_show_separator = pinned_preferred_height != 0 &&
                                     unpinned_preferred_height != 0 &&
                                     is_collapsed_;

  // If the height is bounded, calculate the available space for laying out the
  // pinned and unpinned containers.
  int remaining_height = 0;
  if (size_bounds.height().is_bounded()) {
    remaining_height = size_bounds.height().value();
    if (pinned_preferred_height != 0 && unpinned_preferred_height != 0) {
      remaining_height -= region_vertical_padding;
    }
    if (should_show_separator) {
      remaining_height -= tabs_separator_->GetPreferredSize().height() +
                          region_vertical_padding;
    }
    // Clamp the remaining height to 0 if we have less space.
    remaining_height = std::max(remaining_height, 0);
  }

  // Place the pinned container.
  int pinned_container_height = pinned_preferred_height;
  if (size_bounds.height().is_bounded()) {
    // The pinned container height should not be larger than half the available
    // space unless the unpinned container will not fill that space. Also make
    // sure the height is at least the minimum.
    pinned_container_height = std::max(
        std::min(pinned_preferred_height,
                 std::max(remaining_height / 2,
                          remaining_height - unpinned_preferred_height)),
        pinned_tabs_container_view_->GetMinimumSize().height());
    remaining_height -= pinned_container_height;
  }
  gfx::Rect pinned_container_bounds(
      region_horizontal_padding, y,
      pinned_tab_container_size_bounds.width().value(),
      pinned_container_height);
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view_.get(),
                                     pinned_tabs_scroll_view_->GetVisible(),
                                     pinned_container_bounds);

  if (pinned_container_bounds.height()) {
    y += pinned_container_bounds.height();
    // Add padding only if there are pinned and unpinned tabs.
    if (unpinned_preferred_height != 0) {
      y += region_vertical_padding;
    }
  }

  // Place the tabs separator if visible.
  if (should_show_separator) {
    int separator_width =
        size_bounds.width().value() - 2 * region_horizontal_padding;
    int separator_x = region_horizontal_padding;
    gfx::Rect tabs_separator_bounds(
        separator_x, y, separator_width,
        tabs_separator_->GetPreferredSize().height());
    layouts.child_layouts.emplace_back(tabs_separator_.get(), true,
                                       tabs_separator_bounds);

    y += tabs_separator_bounds.height() + region_vertical_padding;
  } else {
    layouts.child_layouts.emplace_back(tabs_separator_.get(), false,
                                       gfx::Rect());
  }

  // Place the unpinned container using the entire available width, we do not
  // inset the x value by |region_horizontal_padding| here because, when the tab
  // strip is collapsed, tab groups need to draw the group colored line in this
  // space.
  gfx::Rect unpinned_container_bounds(0, y, size_bounds.width().value(),
                                      unpinned_preferred_height);
  if (size_bounds.height().is_bounded()) {
    unpinned_container_bounds.set_height(
        std::max(std::min(unpinned_container_bounds.height(), remaining_height),
                 unpinned_tabs_container_view_->GetMinimumSize().height()));
  }
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view_.get(),
                                     unpinned_tabs_scroll_view_->GetVisible(),
                                     unpinned_container_bounds);

  layouts.host_size = gfx::Size(size_bounds.width().value(),
                                unpinned_container_bounds.bottom());
  return layouts;
}

void VerticalTabStripView::AddedToWidget() {
  views::Widget* const widget = GetWidget();
  paint_as_active_subscription_ =
      widget->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabStripView::UpdateColors, base::Unretained(this)));
  widget_observation_.Observe(widget);
}

void VerticalTabStripView::RemovedFromWidget() {
  widget_observation_.Reset();
}

void VerticalTabStripView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_entered_tabstrip_time_ = base::TimeTicks::Now();
  has_reported_time_mouse_entered_to_switch_ = false;
}

void VerticalTabStripView::OnMouseExited(const ui::MouseEvent& event) {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kHover);
  }
}

void VerticalTabStripView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController();
      hover_card_controller) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  }
}

void VerticalTabStripView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  if (collection_node_ && visible && is_first_window_presentation_) {
    // Only scroll-in the active tab for the first window presentation.
    is_first_window_presentation_ = false;
    OnActiveTabChanged(collection_node_->GetController()->GetActiveTab());
  }
}

void VerticalTabStripView::OnActiveTabChanged(
    const tabs::TabInterface* active_tab) {
  if (collection_node_ && active_tab) {
    // Expand group if the activated tab is within a collapsed group unless
    // we are header dragging the collapsed group.
    if (active_tab->GetGroup().has_value() &&
        !collection_node_->GetController()->GetDragHandler().IsDragging()) {
      TabCollectionNode* group_node = collection_node_->GetNodeForHandle(
          active_tab->GetBrowserWindowInterface()
              ->GetTabStripModel()
              ->group_model()
              ->GetTabGroup(active_tab->GetGroup().value())
              ->GetCollectionHandle());
      CHECK(group_node);

      auto* group_view =
          views::AsViewClass<VerticalTabGroupView>(group_node->view());
      if (group_view && group_view->IsCollapsed()) {
        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMenuAction);
      }
    }

    // Scroll to the activated tab if it isn't in the visible viewport.
    TabCollectionNode* activated_node =
        collection_node_->GetNodeForHandle(active_tab->GetHandle());
    CHECK(activated_node);
    EnsureViewVisible(activated_node->view());
  }
}

void VerticalTabStripView::EnsureViewVisible(views::View* view) {
  if (!view || !Contains(view)) {
    return;
  }

  activated_view_tracker_->SetView(view);

  // Views must either be in the pinned or unpinned view trees.
  DCHECK_NE(pinned_tabs_container_view_->Contains(view),
            unpinned_tabs_container_view_->Contains(view));

  views::ScrollView* const target_scroll_view =
      pinned_tabs_container_view_->Contains(view) ? pinned_tabs_scroll_view_
                                                  : unpinned_tabs_scroll_view_;

  // Wait for the next successful layout before attempting to handle moving
  // the activated view into the scroll view viewport.
  target_scroll_view->RegisterPostLayoutCallback(base::BindRepeating(
      &VerticalTabStripView::EnsureVisibleInViewportPostActivationAndLayout,
      base::Unretained(this)));
}

void VerticalTabStripView::RecordMousePressedInTab() {
  if (mouse_entered_tabstrip_time_.has_value() &&
      !has_reported_time_mouse_entered_to_switch_) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "TabStrip.Vertical.TimeToSwitch",
        base::TimeTicks::Now() - mouse_entered_tabstrip_time_.value());
    has_reported_time_mouse_entered_to_switch_ = true;
  }
}

bool VerticalTabStripView::IsFocusInTabStrip() {
  return GetFocusManager() && Contains(GetFocusManager()->GetFocusedView());
}

VerticalPinnedTabContainerView* VerticalTabStripView::GetPinnedTabsContainer() {
  return pinned_tabs_container_view_;
}

VerticalUnpinnedTabContainerView*
VerticalTabStripView::GetUnpinnedTabsContainer() {
  return unpinned_tabs_container_view_;
}

void VerticalTabStripView::SetCollapsedState(bool is_collapsed) {
  if (is_collapsed != is_collapsed_) {
    is_collapsed_ = is_collapsed;
    InvalidateLayout();
  }
}

void VerticalTabStripView::SetIsAnimatingSize(bool is_animating) {
  for (views::ScrollView* scroll_view :
       {pinned_tabs_scroll_view_, unpinned_tabs_scroll_view_}) {
    if (scroll_view) {
      static_cast<VerticalTabStripScrollBar*>(
          scroll_view->vertical_scroll_bar())
          ->SetIsAnimatingSize(is_animating);
    }
  }
}

bool VerticalTabStripView::IsPositionInWindowCaption(const gfx::Point& point) {
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Point point_in_child = point;
    ConvertPointToTarget(this, child, &point_in_child);
    if (!child->HitTestPoint(point_in_child)) {
      continue;
    }

    auto* scroll_view = views::AsViewClass<views::ScrollView>(child);
    if (!scroll_view) {
      return true;
    }

    if (scroll_view->vertical_scroll_bar()->GetVisible()) {
      gfx::Point point_in_sb = point_in_child;
      ConvertPointToTarget(scroll_view, scroll_view->vertical_scroll_bar(),
                           &point_in_sb);
      if (scroll_view->vertical_scroll_bar()->HitTestPoint(point_in_sb)) {
        return false;
      }
    }

    if (scroll_view->contents()) {
      gfx::Point point_in_content = point_in_child;
      ConvertPointToTarget(scroll_view, scroll_view->contents(),
                           &point_in_content);
      if (scroll_view->contents()->HitTestPoint(point_in_content)) {
        return false;
      }
    }
    return true;
  }

  return true;
}

views::View* VerticalTabStripView::AddScrollViewContents(
    std::unique_ptr<views::View> view) {
  if (auto* container =
          views::AsViewClass<VerticalUnpinnedTabContainerView>(view.get())) {
    unpinned_tabs_container_view_ = container;
    return unpinned_tabs_scroll_view_->SetContents(std::move(view));
  }
  // |view| should only ever be VerticalUnpinnedTabContainerView or
  // VerticalPinnedTabContainerView.
  auto* container =
      views::AsViewClass<VerticalPinnedTabContainerView>(view.get());
  CHECK(container);
  pinned_tabs_container_view_ = container;
  return pinned_tabs_scroll_view_->SetContents(std::move(view));
}

void VerticalTabStripView::RemoveScrollViewContents(views::View* view) {
  if (views::IsViewClass<VerticalUnpinnedTabContainerView>(view)) {
    unpinned_tabs_container_view_ = nullptr;
    unpinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  if (views::IsViewClass<VerticalPinnedTabContainerView>(view)) {
    pinned_tabs_container_view_ = nullptr;
    pinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  // |view| should only ever be VerticalUnpinnedTabContainerView or
  // VerticalPinnedTabContainerView.
  NOTREACHED();
}

void VerticalTabStripView::SetScrollViewProperties(
    views::ScrollView* scroll_view) {
  scroll_view->SetUseContentsPreferredSize(true);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetOverflowGradientMask(
      views::ScrollView::GradientDirection::kVertical);
  CHECK(collection_node_);
  scroll_view->SetVerticalScrollBar(std::make_unique<VerticalTabStripScrollBar>(
      collection_node_->GetController()->GetStateController()));
  callback_subscriptions_.emplace_back(scroll_view->AddContentsScrolledCallback(
      base::BindRepeating(&VerticalTabStripView::HideHoverCardOnScroll,
                          base::Unretained(this))));
}

void VerticalTabStripView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabStripView::EnsureVisibleInViewportPostActivationAndLayout(
    views::ScrollView* scroll_view) {
  // Explicitly re-register only as needed.
  scroll_view->RegisterPostLayoutCallback(base::DoNothing());

  // Guard against views being removed from the tree between frames. Dragging a
  // view out of the visible bounds will also trigger a scroll naturally.
  views::View* const activated_view = activated_view_tracker_->view();
  if (!activated_view || !Contains(activated_view) ||
      (collection_node_ &&
       collection_node_->GetController()->GetDragHandler().IsDragging())) {
    EnableOverflowVisuals(scroll_view);
    return;
  }

  // Handle the case where the scroll view is currently not in an overflow
  // state. In such a case the activated view will be visible in the scroll
  // view's viewport without scrolling.
  if (!scroll_view->IsVerticalContentOverflowing()) {
    // It may be the case that the activated view is not at its target height
    // (i.e. it was activated as it is being animated in). In such a case
    // disable overflow visuals to prevent jank that can occur if content view
    // bounds are changed in quick succession.
    if (!activated_view_tracker_->IsViewAtPreferredHeight()) {
      DisableOverflowVisuals(scroll_view);
      activated_view_tracker_->SetOnReachedPreferredHeightCallback(
          base::BindOnce(&VerticalTabStripView::
                             EnsureVisibleInViewportPostActivationAndLayout,
                         base::Unretained(this), scroll_view));
    } else {
      // Always exit with overflow visuals enabled.
      EnableOverflowVisuals(scroll_view);
    }
    return;
  }

  // Get view bounds in its contents coordinates.
  gfx::Rect activated_view_bounds =
      GetVerticalTabStripViewTargetBounds(activated_view);

  // Proceed up the hierarchy until the content view is reached, iteratively
  // adjusting target view bounds.
  for (views::View* v = activated_view->parent();
       v && v != scroll_view->contents(); v = v->parent()) {
    activated_view_bounds =
        views::View::ConvertRectToTarget(v, v->parent(), activated_view_bounds);
  }

  // Determine the adjustment required to fit the activated view into the
  // visible content view bounds.
  gfx::Rect adjusted_activated_view_bounds = activated_view_bounds;
  adjusted_activated_view_bounds.AdjustToFit(scroll_view->GetVisibleRect());

  // Calculate the required scroll offset for the visible content bounds (the
  // reverse of the activated view adjustment).
  const int diff =
      activated_view_bounds.y() - adjusted_activated_view_bounds.y();

  // Calculate the required scroll offset for the visible content bounds taking
  // into account configured overflow gradients. This is deliberately more than
  // is needed and may or may not apply depending on view position.
  gfx::Rect overflow_adjusted_activated_view_bounds = activated_view_bounds;
  overflow_adjusted_activated_view_bounds.AdjustToFit(
      scroll_view->GetOpaqueVisibleRect());
  const int diff_avoid_overflow_gradient =
      activated_view_bounds.y() - overflow_adjusted_activated_view_bounds.y();

  if (diff != 0) {
    // Disable overflow visuals to avoid visual artifacts while scrolling,
    // particularly for views towards the bottom of the scroll view.
    DisableOverflowVisuals(scroll_view);
    scroll_view->ScrollByOffset(
        {0, static_cast<float>(diff_avoid_overflow_gradient)});
    scroll_view->RegisterPostLayoutCallback(base::BindRepeating(
        &VerticalTabStripView::EnsureVisibleInViewportPostActivationAndLayout,
        base::Unretained(this)));
    scroll_view->InvalidateLayout();
  } else {
    // Request a final scroll to ensure the activated view is moved beyond the
    // overflow gradient if necessary.
    scroll_view->ScrollByOffset(
        {0, static_cast<float>(diff_avoid_overflow_gradient)});
    EnableOverflowVisuals(scroll_view);
  }
}

void VerticalTabStripView::EnableOverflowVisuals(
    views::ScrollView* scroll_view) {
  // Override the post-layout callback to prevent any scheduled scroll requests
  // from running.
  scroll_view->RegisterPostLayoutCallback(base::DoNothing());
  scroll_view->SetDrawOverflowIndicator(true);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);

  // Restore normal VerticalTabStripScrollBar scrollbar behavior.
  if (auto* scroll_bar = views::AsViewClass<VerticalTabStripScrollBar>(
          scroll_view->vertical_scroll_bar())) {
    scroll_bar->SetIsAnimatingSize(false);
  }

  // Reset the active view as it is no longer needed after post-activation
  // adjustment for viewport visibility is complete.
  activated_view_tracker_->SetView(nullptr);
  scroll_view->InvalidateLayout();
}

void VerticalTabStripView::DisableOverflowVisuals(
    views::ScrollView* scroll_view) {
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // Suppress scrollbar visuals to avoid artifacts as views are resized to
  // target bounds whilst simultaneously scrolling to target.
  if (auto* scroll_bar = views::AsViewClass<VerticalTabStripScrollBar>(
          scroll_view->vertical_scroll_bar())) {
    scroll_bar->SetIsAnimatingSize(true);
  }
}

void VerticalTabStripView::UpdateColors() {
  tabs_separator_->SetColorId(IsFrameActive() ? kColorTabDividerFrameActive
                                              : kColorTabDividerFrameInactive);
}

bool VerticalTabStripView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

void VerticalTabStripView::HideHoverCardOnScroll() {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController();
      hover_card_controller && hover_card_controller->IsHoverCardVisible()) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kAnimating);
  }
}

BEGIN_METADATA(VerticalTabStripView)
END_METADATA
