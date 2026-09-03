// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/horizontal_tab_strip_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view_layout.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_strip_overflow_indicator_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_scroll_bar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

bool IsVerticalOrientation(const TabCollectionNode* collection_node) {
  return collection_node &&
         collection_node->orientation() == TabStripOrientation::kVertical;
}

}  // namespace

class TabStripView::TargetViewsTracker : public views::ViewObserver {
 public:
  explicit TargetViewsTracker(TabStripOrientation orientation)
      : orientation_(orientation) {}
  TargetViewsTracker(const TargetViewsTracker&) = delete;
  TargetViewsTracker& operator=(const TargetViewsTracker&) = delete;
  ~TargetViewsTracker() override = default;

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    RemoveView(observed_view);
  }
  void OnViewRemovedFromWidget(views::View* observed_view) override {
    RemoveView(observed_view);
  }
  void OnViewBoundsChanged(views::View* observed_view) override {
    CheckTrackedViewsSize();
  }
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    CheckTrackedViewsSize();
  }

  void SetViews(views::View* primary_view,
                views::View* secondary_view = nullptr) {
    Clear();
    if (primary_view) {
      primary_view_ = primary_view;
      observations_.AddObservation(primary_view);
    }
    if (secondary_view && secondary_view != primary_view) {
      secondary_view_ = secondary_view;
      observations_.AddObservation(secondary_view);
    }
  }

  void RemoveView(views::View* view) {
    if (!view || !observations_.IsObservingSource(view)) {
      return;
    }
    if (view == primary_view_) {
      primary_view_ = nullptr;
    }
    if (view == secondary_view_) {
      secondary_view_ = nullptr;
    }
    observations_.RemoveObservation(view);
    if (!observations_.IsObservingAnySource()) {
      on_reached_target_size_cb_.Reset();
    }
  }

  void RemoveViewsInContainer(const views::View* container) {
    if (!container) {
      return;
    }
    if (primary_view_ && container->Contains(primary_view_)) {
      RemoveView(primary_view_);
    }
    if (secondary_view_ && container->Contains(secondary_view_)) {
      RemoveView(secondary_view_);
    }
  }

  void Clear() {
    primary_view_ = nullptr;
    secondary_view_ = nullptr;
    observations_.RemoveAllObservations();
    on_reached_target_size_cb_.Reset();
  }

  views::View* primary_view() const { return primary_view_; }
  views::View* secondary_view() const { return secondary_view_; }
  bool empty() const { return !observations_.IsObservingAnySource(); }

  // Returns true if all tracked views are at their target size based on
  // orientation.
  bool AreViewsAtTargetSize() const {
    for (views::View* view : {primary_view_.get(), secondary_view_.get()}) {
      if (!view) {
        continue;
      }
      if (orientation_ == TabStripOrientation::kVertical) {
        if (view->height() != view->GetPreferredSize().height()) {
          return false;
        }
      } else {
        if (view->width() != GetTabStripViewTargetBounds(view).width()) {
          return false;
        }
      }
    }
    return true;
  }

  void SetOnReachedTargetSizeCallback(
      base::OnceClosure on_reached_target_size_cb) {
    on_reached_target_size_cb_ = std::move(on_reached_target_size_cb);
    CheckTrackedViewsSize();
  }

 private:
  void CheckTrackedViewsSize() {
    if (observations_.IsObservingAnySource() && AreViewsAtTargetSize() &&
        on_reached_target_size_cb_) {
      std::move(on_reached_target_size_cb_).Run();
    }
  }

  const TabStripOrientation orientation_;
  raw_ptr<views::View> primary_view_ = nullptr;
  raw_ptr<views::View> secondary_view_ = nullptr;
  base::OnceClosure on_reached_target_size_cb_;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      observations_{this};
};

TabStripView::TabStripView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      target_views_tracker_(std::make_unique<TargetViewsTracker>(
          collection_node->orientation())) {
  // Paint to a layer and mask to bounds to prevent tabs from overflowing and
  // drawing outside the window boundaries on Linux when the window is small.
  // This is configured here rather than a higher-level container so that drop
  // shadows are not clipped.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  SetLayoutManager(
      std::make_unique<TabStripViewLayout>(collection_node->orientation()));
  SetProperty(views::kElementIdentifierKey, kTabStripElementId);

  pinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(pinned_tabs_scroll_view_);

  auto tabs_separator = std::make_unique<views::Separator>();
  tabs_separator_ = AddChildView(std::move(tabs_separator));

  unpinned_tabs_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  SetScrollViewProperties(unpinned_tabs_scroll_view_);

  if (!IsVerticalOrientation(collection_node_)) {
    unpinned_tab_scroll_type_recorder_ =
        std::make_unique<tabs::UnpinnedTabScrollTypeRecorder>(
            unpinned_tabs_scroll_view_);
    unpinned_tab_scrollable_state_recorder_ =
        std::make_unique<tabs::UnpinnedTabScrollableStateRecorder>(
            unpinned_tabs_scroll_view_);
    tab_scroll_button_container_ =
        AddChildView(std::make_unique<TabScrollButtonContainer>(
            collection_node_->GetController()->GetBrowserView()->browser()));
    tab_scroll_button_container_->SetScrollView(unpinned_tabs_scroll_view_);
  }

  collection_node->set_add_child_to_node(base::BindRepeating(
      &TabStripView::AddScrollViewContents, base::Unretained(this)));

  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabStripView::RemoveScrollViewContents, base::Unretained(this)));

  callback_subscriptions_.emplace_back(
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &TabStripView::ResetCollectionNode, base::Unretained(this))));

  if (tab_scroll_button_container_) {
    if (PrefService* prefs = GetPrefs()) {
      pref_change_registrar_.Init(prefs);
      pref_change_registrar_.Add(
          prefs::kTabScrollButtonsPinnedToTabstrip,
          base::BindRepeating(&TabStripView::InvalidateLayout,
                              base::Unretained(this), false));
    }
  }

  SetNotifyEnterExitOnChild(true);
  UpdateColors();
}

TabStripView::~TabStripView() {
  if (tab_scroll_button_container_) {
    tab_scroll_button_container_->SetScrollView(nullptr);
  }
}

void TabStripView::AddedToWidget() {
  views::Widget* const widget = GetWidget();
  paint_as_active_subscription_ = widget->RegisterPaintAsActiveChangedCallback(
      base::BindRepeating(&TabStripView::UpdateColors, base::Unretained(this)));
  widget_observation_.Observe(widget);
}

void TabStripView::RemovedFromWidget() {
  widget_observation_.Reset();
}

void TabStripView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_entered_tabstrip_time_ = base::TimeTicks::Now();
  has_reported_time_mouse_entered_to_switch_ = false;
}

void TabStripView::OnMouseExited(const ui::MouseEvent& event) {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kHover);
  }
}

void TabStripView::OnWidgetActivationChanged(views::Widget* widget,
                                             bool active) {
  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController();
      hover_card_controller) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  }
}

void TabStripView::OnChildMoved(TabCollectionNode* moved_node) {
  CHECK(moved_node);

  if (collection_node_ &&
      collection_node_->GetController()->GetDragHandler().IsDragging()) {
    return;
  }

  const tabs::TabInterface* active_tab =
      collection_node_->GetController()->GetActiveTab();

  bool is_active_tab = false;
  if (active_tab) {
    if (moved_node->type() == TabCollectionNode::Type::TAB) {
      const tabs::TabInterface* tab =
          std::get<tabs::ConstDanglingUntriagedTabInterface>(
              moved_node->GetNodeData());
      is_active_tab = (tab == active_tab);
    } else if (moved_node->type() == TabCollectionNode::Type::SPLIT) {
      is_active_tab =
          (moved_node->GetNodeForHandle(active_tab->GetHandle()) != nullptr);
    }
  }

  const views::View* focused_view =
      GetFocusManager() ? GetFocusManager()->GetFocusedView() : nullptr;
  views::View* view = moved_node->view();
  bool has_focus = view && focused_view && view->Contains(focused_view);

  if (is_active_tab || has_focus) {
    switch (moved_node->type()) {
      case TabCollectionNode::Type::TAB: {
        const tabs::TabInterface* tab =
            std::get<tabs::ConstDanglingUntriagedTabInterface>(
                moved_node->GetNodeData());
        OnTabChanged(tab);
        break;
      }
      case TabCollectionNode::Type::SPLIT:
      case TabCollectionNode::Type::GROUP:
        ScrollToFitViews(view, nullptr);
        break;
      case TabCollectionNode::Type::TABSTRIP:
      case TabCollectionNode::Type::PINNED:
      case TabCollectionNode::Type::UNPINNED:
        break;
    }
  }
}

void TabStripView::OnTabChanged(const tabs::TabInterface* active_tab) {
  if (!collection_node_ || !active_tab) {
    return;
  }

  if (collection_node_->GetController()->GetDragHandler().IsDragging()) {
    return;
  }

  // Expand group if the activated tab is within a collapsed group unless
  // we are header dragging the collapsed group.
  if (active_tab->GetGroup().has_value()) {
    TabCollectionNode* group_node = collection_node_->GetNodeForHandle(
        active_tab->GetBrowserWindowInterface()
            ->GetTabStripModel()
            ->group_model()
            ->GetTabGroup(active_tab->GetGroup().value())
            ->GetCollectionHandle());
    if (group_node) {
      auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
      if (group_view && group_view->IsCollapsed()) {
        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMenuAction);
      }
    }
  }

  TabCollectionNode* activated_node =
      collection_node_->GetNodeForHandle(active_tab->GetHandle());
  CHECK(activated_node);
  ScrollToFitViews(activated_node->view(), nullptr);
}

void TabStripView::ScrollToFitTabs(const tabs::TabInterface* active_tab,
                                   const tabs::TabInterface* new_tab) {
  if (!collection_node_ || !active_tab || !new_tab) {
    return;
  }

  if (collection_node_->GetController()->GetDragHandler().IsDragging()) {
    return;
  }

  TabCollectionNode* active_node =
      collection_node_->GetNodeForHandle(active_tab->GetHandle());
  TabCollectionNode* new_node =
      collection_node_->GetNodeForHandle(new_tab->GetHandle());

  if (!active_node || !new_node) {
    return;
  }

  views::View* active_view = active_node->view();
  views::View* new_view = new_node->view();

  if (!active_view || !new_view) {
    return;
  }

  ScrollToFitViews(active_view, new_view);
}

void TabStripView::ScrollToFitViews(views::View* view1, views::View* view2) {
  views::View* primary_view = (view1 && Contains(view1)) ? view1 : nullptr;
  views::View* secondary_view = (view2 && Contains(view2)) ? view2 : nullptr;
  if (!primary_view && !secondary_view) {
    return;
  }

  // If a scroll request is active, restore normal overflow visuals and clear
  // pending callbacks before servicing the new target views.
  if (!target_views_tracker_->empty()) {
    EnableOverflowVisuals(pinned_tabs_scroll_view_);
    EnableOverflowVisuals(unpinned_tabs_scroll_view_);
  }

  target_views_tracker_->SetViews(primary_view, secondary_view);

  bool has_pinned = false;
  bool has_unpinned = false;
  if (primary_view) {
    if (pinned_tabs_container_view_->Contains(primary_view)) {
      has_pinned = true;
    } else {
      has_unpinned = true;
    }
  }
  if (secondary_view) {
    if (pinned_tabs_container_view_->Contains(secondary_view)) {
      has_pinned = true;
    } else {
      has_unpinned = true;
    }
  }

  if (has_pinned) {
    pinned_tabs_scroll_view_->RegisterPostLayoutCallback(base::BindRepeating(
        &TabStripView::EnsureViewsVisibleInViewportPostLayout,
        base::Unretained(this)));
  }
  if (has_unpinned) {
    unpinned_tabs_scroll_view_->RegisterPostLayoutCallback(base::BindRepeating(
        &TabStripView::EnsureViewsVisibleInViewportPostLayout,
        base::Unretained(this)));
  }
}

void TabStripView::RecordMousePressedInTab() {
  if (mouse_entered_tabstrip_time_.has_value() &&
      !has_reported_time_mouse_entered_to_switch_) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "TabStrip.Vertical.TimeToSwitch",
        base::TimeTicks::Now() - mouse_entered_tabstrip_time_.value());
    has_reported_time_mouse_entered_to_switch_ = true;
  }
}

bool TabStripView::IsFocusInTabStrip() {
  return GetFocusManager() && Contains(GetFocusManager()->GetFocusedView());
}

views::View* TabStripView::GetPrimaryScrollTargetForTesting() const {
  return target_views_tracker_ ? target_views_tracker_->primary_view()
                               : nullptr;
}

views::View* TabStripView::GetSecondaryScrollTargetForTesting() const {
  return target_views_tracker_ ? target_views_tracker_->secondary_view()
                               : nullptr;
}

PinnedTabContainerView* TabStripView::GetPinnedTabsContainer() const {
  return pinned_tabs_container_view_;
}

UnpinnedTabContainerView* TabStripView::GetUnpinnedTabsContainer() const {
  return unpinned_tabs_container_view_;
}

TabScrollButtonContainer* TabStripView::GetScrollButtonContainer() const {
  return tab_scroll_button_container_;
}

bool TabStripView::IsTabScrollButtonsPinned() const {
  PrefService* prefs = GetPrefs();
  return prefs ? prefs->GetBoolean(prefs::kTabScrollButtonsPinnedToTabstrip)
               : true;
}

void TabStripView::SetCollapsedState(bool is_collapsed) {
  if (is_collapsed != is_collapsed_) {
    is_collapsed_ = is_collapsed;
    InvalidateLayout();
  }
}

void TabStripView::SetIsAnimatingSize(bool is_animating) {
  for (views::ScrollView* scroll_view :
       {pinned_tabs_scroll_view_, unpinned_tabs_scroll_view_}) {
    // Scrollbars are not visible in the horizontal orientation so only update
    // for the vertical orientation.
    if (scroll_view && IsVerticalOrientation(collection_node_)) {
      if (auto* scroll_bar = views::AsViewClass<VerticalTabStripScrollBar>(
              scroll_view->vertical_scroll_bar())) {
        scroll_bar->SetIsAnimatingSize(is_animating);
      }
    }
  }
}

bool TabStripView::IsPositionInWindowCaption(const gfx::Point& point) {
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Point point_in_child = point;
    ConvertPointToTarget(this, child, &point_in_child);
    if (!child->HitTestPoint(point_in_child)) {
      continue;
    }

    if (TabScrollButtonContainer* scroll_button_container =
            views::AsViewClass<TabScrollButtonContainer>(child)) {
      return scroll_button_container->IsPositionInWindowCaption(point_in_child);
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

views::View* TabStripView::AddScrollViewContents(
    std::unique_ptr<views::View> view) {
  if (auto* container =
          views::AsViewClass<UnpinnedTabContainerView>(view.get())) {
    unpinned_tabs_container_view_ = container;
    return unpinned_tabs_scroll_view_->SetContents(std::move(view));
  }
  // `view` should only ever be UnpinnedTabContainerView or
  // PinnedTabContainerView.
  auto* container = views::AsViewClass<PinnedTabContainerView>(view.get());
  CHECK(container);
  pinned_tabs_container_view_ = container;
  return pinned_tabs_scroll_view_->SetContents(std::move(view));
}

void TabStripView::RemoveScrollViewContents(views::View* view) {
  if (views::IsViewClass<UnpinnedTabContainerView>(view)) {
    unpinned_tabs_container_view_ = nullptr;
    unpinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  if (views::IsViewClass<PinnedTabContainerView>(view)) {
    pinned_tabs_container_view_ = nullptr;
    pinned_tabs_scroll_view_->SetContents(nullptr);
    return;
  }
  // `view` should only ever be UnpinnedTabContainerView or
  // PinnedTabContainerView.
  NOTREACHED();
}

void TabStripView::SetScrollViewProperties(views::ScrollView* scroll_view) {
  scroll_view->SetUseContentsPreferredSize(true);
  scroll_view->SetBackgroundColor(std::nullopt);
  CHECK(collection_node_);
  if (IsVerticalOrientation(collection_node_)) {
    scroll_view->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll_view->SetOverflowGradientMask(
        views::ScrollView::GradientDirection::kVertical);
    scroll_view->SetVerticalScrollBar(
        std::make_unique<VerticalTabStripScrollBar>(
            collection_node_->GetController()->GetStateController()));
  } else {
    scroll_view->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    scroll_view->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll_view->SetTreatAllScrollEventsAsHorizontal(true);

    scroll_view->SetCustomOverflowIndicator(
        views::OverflowIndicatorAlignment::kLeft,
        std::make_unique<HorizontalTabStripOverflowIndicatorView>(
            views::OverflowIndicatorAlignment::kLeft),
        HorizontalTabStripOverflowIndicatorView::kTotalThickness,
        /*fills_opaquely=*/false);

    scroll_view->SetCustomOverflowIndicator(
        views::OverflowIndicatorAlignment::kRight,
        std::make_unique<HorizontalTabStripOverflowIndicatorView>(
            views::OverflowIndicatorAlignment::kRight),
        HorizontalTabStripOverflowIndicatorView::kTotalThickness,
        /*fills_opaquely=*/false);
  }
  callback_subscriptions_.emplace_back(
      scroll_view->AddContentsScrolledCallback(base::BindRepeating(
          &TabStripView::HideHoverCardOnScroll, base::Unretained(this))));
}

void TabStripView::ResetCollectionNode() {
  collection_node_ = nullptr;
  if (tab_scroll_button_container_) {
    tab_scroll_button_container_->SetScrollView(nullptr);
  }
  pref_change_registrar_.Reset();
}

PrefService* TabStripView::GetPrefs() const {
  BrowserView* browser_view =
      collection_node_ ? collection_node_->GetController()->GetBrowserView()
                       : nullptr;
  if (!browser_view || !browser_view->browser()) {
    return nullptr;
  }
  Profile* profile = browser_view->browser()->GetProfile();
  return profile ? profile->GetPrefs() : nullptr;
}

void TabStripView::EnsureViewsVisibleInViewportPostLayout(
    views::ScrollView* scroll_view) {
  // Explicitly re-register only as needed.
  scroll_view->RegisterPostLayoutCallback(base::DoNothing());

  // Guard against views being removed from the tree between frames. Dragging a
  // view out of the visible bounds will also trigger a scroll naturally.
  if (!target_views_tracker_ || target_views_tracker_->empty() ||
      (collection_node_ &&
       collection_node_->GetController()->GetDragHandler().IsDragging())) {
    EnableOverflowVisuals(scroll_view);
    return;
  }

  gfx::Rect combined_bounds;
  bool has_valid_views = false;

  for (views::View* view : {target_views_tracker_->primary_view(),
                            target_views_tracker_->secondary_view()}) {
    if (view && Contains(view) && scroll_view->contents()->Contains(view)) {
      gfx::Rect view_bounds = GetBoundsInScrollViewContents(view, scroll_view);
      if (!has_valid_views) {
        combined_bounds = view_bounds;
        has_valid_views = true;
      } else {
        combined_bounds.Union(view_bounds);
      }
    }
  }

  if (!has_valid_views) {
    EnableOverflowVisuals(scroll_view);
    return;
  }

  const bool is_vertical = IsVerticalOrientation(collection_node_);

  // Handle the case where the scroll view is currently not in an overflow
  // state. In such a case the tracked views will be visible in the scroll
  // view's viewport without scrolling.
  const bool is_overflowing =
      is_vertical ? scroll_view->IsVerticalContentOverflowing()
                  : scroll_view->IsHorizontalContentOverflowing();
  if (!is_overflowing) {
    // It may be the case that a tracked view is not at its target height
    // (i.e. it was activated as it is being animated in). In such a case
    // disable overflow visuals to prevent jank that can occur if content view
    // bounds are changed in quick succession.
    if (!target_views_tracker_->AreViewsAtTargetSize()) {
      DisableOverflowVisuals(scroll_view);
      target_views_tracker_->SetOnReachedTargetSizeCallback(
          base::BindOnce(&TabStripView::EnsureViewsVisibleInViewportPostLayout,
                         base::Unretained(this), scroll_view));
    } else {
      // Always exit with overflow visuals enabled.
      EnableOverflowVisuals(scroll_view);
    }
    return;
  }

  // If the combined bounds don't fit in the viewport, fall back to prioritizing
  // the active (primary) tab. If the primary tab belongs to a different scroll
  // view (e.g. pinned tab while checking the unpinned scroll view),
  // `Contains(primary_view)` is false, safely skipping this fallback check for
  // this scroll view.
  const int combined_size =
      is_vertical ? combined_bounds.height() : combined_bounds.width();
  const int viewport_size = is_vertical ? scroll_view->GetVisibleRect().height()
                                        : scroll_view->GetVisibleRect().width();
  if (combined_size > viewport_size && !target_views_tracker_->empty()) {
    views::View* primary_view = target_views_tracker_->primary_view();
    if (primary_view && scroll_view->contents()->Contains(primary_view)) {
      combined_bounds =
          GetBoundsInScrollViewContents(primary_view, scroll_view);
    }
  }

  // Determine the adjustment required to fit the combined target views bounds
  // into the visible content view bounds.
  gfx::Rect adjusted_bounds = combined_bounds;
  adjusted_bounds.AdjustToFit(scroll_view->GetVisibleRect());

  // Calculate the required scroll offset for the visible content bounds (the
  // reverse of the target bounds adjustment).
  const int diff = is_vertical ? combined_bounds.y() - adjusted_bounds.y()
                               : combined_bounds.x() - adjusted_bounds.x();

  // Calculate the required scroll offset taking into account configured
  // overflow gradients. This is deliberately more than is needed and may or
  // may not apply depending on view position.
  gfx::Rect overflow_adjusted_bounds = combined_bounds;
  overflow_adjusted_bounds.AdjustToFit(scroll_view->GetOpaqueVisibleRect());
  const int diff_avoid_overflow_gradient =
      is_vertical ? combined_bounds.y() - overflow_adjusted_bounds.y()
                  : combined_bounds.x() - overflow_adjusted_bounds.x();

  const gfx::PointF scroll_offset =
      is_vertical
          ? gfx::PointF(0, static_cast<float>(diff_avoid_overflow_gradient))
          : gfx::PointF(static_cast<float>(diff_avoid_overflow_gradient), 0);

  if (diff != 0) {
    // Disable overflow visuals to avoid visual artifacts while scrolling.
    DisableOverflowVisuals(scroll_view);
    scroll_view->ScrollByOffset(scroll_offset);
    scroll_view->RegisterPostLayoutCallback(base::BindRepeating(
        &TabStripView::EnsureViewsVisibleInViewportPostLayout,
        base::Unretained(this)));
    scroll_view->InvalidateLayout();
  } else {
    // Request a final scroll to ensure the tracked views are moved beyond the
    // overflow gradient if necessary.
    scroll_view->ScrollByOffset(scroll_offset);
    EnableOverflowVisuals(scroll_view);
  }
}

void TabStripView::EnableOverflowVisuals(views::ScrollView* scroll_view) {
  // Override the post-layout callback to prevent any scheduled scroll requests
  // from running.
  scroll_view->RegisterPostLayoutCallback(base::DoNothing());
  if (IsVerticalOrientation(collection_node_)) {
    scroll_view->SetDrawOverflowIndicator(true);
    scroll_view->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kEnabled);

    // Restore normal VerticalTabStripScrollBar scrollbar behavior.
    if (auto* scroll_bar = views::AsViewClass<VerticalTabStripScrollBar>(
            scroll_view->vertical_scroll_bar())) {
      scroll_bar->SetIsAnimatingSize(false);
    }
  }

  // Reset the active view as it is no longer needed after post-activation
  // adjustment for viewport visibility is complete.
  if (target_views_tracker_) {
    target_views_tracker_->RemoveViewsInContainer(scroll_view->contents());
  }
  scroll_view->InvalidateLayout();
}

void TabStripView::DisableOverflowVisuals(views::ScrollView* scroll_view) {
  // If in the vertical orientation also hide scrollbar visuals. This is not
  // needed for horizontal since the scrollbar is not shown.
  if (IsVerticalOrientation(collection_node_)) {
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
}

gfx::Rect TabStripView::GetBoundsInScrollViewContents(
    views::View* view,
    views::ScrollView* scroll_view) {
  gfx::Rect bounds = GetTabStripViewTargetBounds(view);
  for (views::View* v = view->parent(); v && v != scroll_view->contents();
       v = v->parent()) {
    bounds = views::View::ConvertRectToTarget(v, v->parent(), bounds);
  }
  return bounds;
}

void TabStripView::UpdateColors() {
  if (tabs_separator_) {
    tabs_separator_->SetColorId(IsFrameActive()
                                    ? kColorTabDividerFrameActive
                                    : kColorTabDividerFrameInactive);
  }
}

bool TabStripView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

void TabStripView::HideHoverCardOnScroll() {
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

void TabStripView::SetAvailableUnpinnedSpace(views::SizeBound space) const {
  if (unpinned_tabs_container_view_) {
    unpinned_tabs_container_view_->SetAvailableSpace(space);
  }
}

BEGIN_METADATA(TabStripView)
END_METADATA
