// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include <algorithm>
#include <optional>
#include <variant>

#include "base/callback_list.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/background.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kRegionVerticalPadding = 5;
constexpr int kResizeAreaWidth = 5;
constexpr int kCollapsedResizeAreaWidth = 2;
}  // namespace

VerticalTabStripRegionView::VerticalTabStripRegionView(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserView* browser_view)
    : browser_view_(browser_view),
      resize_area_width_(kResizeAreaWidth),
      tab_strip_model_(browser_view->browser()->GetTabStripModel()),
      state_controller_(state_controller),
      root_action_item_(root_action_item),
      hover_card_controller_(
          std::make_unique<TabHoverCardController>(this,
                                                   browser_view->browser())),
      resize_animation_(this) {
  // For z-ordering purposes this needs to be on a layer.
  SetPaintToLayer();
  // Because corners may be transparent, this must be set to false.
  layer()->SetFillsBoundsOpaquely(false);

  flex_layout_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kVertical,
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  // Create child views.
  top_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripTopContainer>(
          state_controller_, root_action_item, browser_view->browser()));

  top_button_separator_ = AddChildView(std::make_unique<views::Separator>());

  bottom_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripBottomContainer>(
          state_controller_, root_action_item,
          base::BindRepeating(
              &VerticalTabStripRegionView::RecordNewTabButtonPressed,
              base::Unretained(this))));
  bottom_button_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  gemini_button_ = AddChildView(std::make_unique<views::View>());

  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));
  resize_area_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  resize_animation_.SetSlideDuration(gfx::Animation::RichAnimationDuration(
      features::UseSidePanelFlyoverAnimation() ? base::Milliseconds(350)
                                               : base::Milliseconds(450)));
  resize_animation_.SetTweenType(
      features::UseSidePanelFlyoverAnimation()
          ? gfx::Tween::Type::ACCEL_30_DECEL_20_85
          : gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);
  resize_animation_.Reset(!state_controller_->IsCollapsed());

  target_collapse_state_ = state_controller_->GetState();
  OnCollapsedStateChanged(state_controller_);
  collapsed_state_changed_subscription_ =
      state_controller_->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripRegionView::OnCollapsedStateChanged,
          base::Unretained(this)));

  SetProperty(views::kElementIdentifierKey, kVerticalTabStripRegionElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);

  SetBackground(std::make_unique<CustomCornersBackground>(
      *this, *browser_view,
      /*primary_color=*/CustomCornersBackground::FrameTheme(),
      /*corner_color=*/CustomCornersBackground::ToolbarTheme()));

  UpdateColors();
}

VerticalTabStripRegionView::~VerticalTabStripRegionView() {
  if (root_node_) {
    root_node_->SetController(nullptr);
  }

  tab_strip_controller_.reset();

  if (drag_handler_) {
    auto handler = RemoveChildViewT(drag_handler_->GetDragContext());
    drag_handler_ = nullptr;
  }
}

std::optional<double> VerticalTabStripRegionView::GetCollapseAnimationPercent()
    const {
  return resize_animation_.is_animating()
             ? std::make_optional(resize_animation_.GetCurrentValue())
             : std::nullopt;
}

void VerticalTabStripRegionView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabStripRegionView::UpdateColors, base::Unretained(this)));
}

void VerticalTabStripRegionView::Layout(PassKey) {
  LayoutSuperclass<views::AccessiblePaneView>(this);

  // Manually position the resize area as it overlaps views handled by the flex
  // layout.
  resize_area_->SetBoundsRect(gfx::Rect(bounds().right() - resize_area_width_,
                                        0, resize_area_width_,
                                        bounds().height()));
}

views::View* VerticalTabStripRegionView::GetDefaultFocusableChild() {
  const int active_index = tab_strip_model_->active_index();
  if (active_index != TabStripModel::kNoTab) {
    return GetTabAnchorViewAt(active_index);
  }

  return top_button_container_;
}

void VerticalTabStripRegionView::InitializeTabStrip() {
  if (root_node_) {
    return;
  }

  root_node_ = std::make_unique<RootTabCollectionNode>(
      tab_strip_model_,
      base::BindRepeating(&VerticalTabStripRegionView::SetTabStripView,
                          base::Unretained(this)),
      base::BindRepeating(&VerticalTabStripRegionView::ClearTabStripView,
                          base::Unretained(this)));

  std::unique_ptr<TabMenuModelFactory> tab_menu_model_factory;
  if (browser_view_ && browser_view_->browser()->app_controller()) {
    tab_menu_model_factory =
        browser_view_->browser()->app_controller()->GetTabMenuModelFactory();
  }

  TabStripModel* tab_strip_model = browser_view_->browser()->GetTabStripModel();
  CHECK(tab_strip_model);
  auto drag_handler = std::make_unique<VerticalTabDragHandlerImpl>(
      *tab_strip_model, *root_node_.get());
  drag_handler_ = drag_handler.get();

  CHECK(!tab_strip_controller_);
  tab_strip_controller_ = std::make_unique<VerticalTabStripController>(
      tab_strip_model, browser_view_, *AddChildView(std::move(drag_handler)),
      hover_card_controller_.get(), std::move(tab_menu_model_factory));

  root_node_->SetController(tab_strip_controller_.get());

  root_node_->Init();

  new_tab_button_pressed_start_time_ = std::nullopt;
  on_children_added_subscription_ = root_node_->RegisterOnChildrenAddedCallback(
      base::BindRepeating(&VerticalTabStripRegionView::OnChildrenAdded,
                          base::Unretained(this)));
}

void VerticalTabStripRegionView::ResetTabStrip() {
  if (!root_node_) {
    return;
  }

  on_children_added_subscription_.reset();

  root_node_->Reset();

  root_node_->SetController(nullptr);
  tab_strip_controller_.reset();

  CHECK(drag_handler_);
  auto* drag_handler = drag_handler_.get();
  drag_handler_ = nullptr;
  RemoveChildViewT(drag_handler->GetDragContext());

  root_node_.reset();
}

gfx::Size VerticalTabStripRegionView::GetMinimumSize() const {
  auto min_size = TabStripRegionView::GetMinimumSize();
  min_size.set_width(
      (state_controller_->IsCollapsed() || resize_animation_.is_animating())
          ? kCollapsedWidth
          : kUncollapsedMinWidth);
  return min_size;
}

gfx::Size VerticalTabStripRegionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto size = TabStripRegionView::CalculatePreferredSize(available_size);
  if (resize_animation_.is_animating()) {
    size.set_width(
        kCollapsedWidth +
        (resize_animation_.IsShowing()
             ? std::floor<double>
             : std::ceil<double>)((target_collapse_state_.uncollapsed_width -
                                   kCollapsedWidth) *
                                  resize_animation_.GetCurrentValue()));
  } else {
    size.set_width(target_collapse_state_.collapsed
                       ? kCollapsedWidth
                       : target_collapse_state_.uncollapsed_width);
  }
  return size;
}

bool VerticalTabStripRegionView::IsTabStripEditable() const {
  return tab_strip_editable_for_testing_ &&
         (!drag_handler_ ||
          !drag_handler_->GetDragContext()->GetDragController());
}

void VerticalTabStripRegionView::DisableTabStripEditingForTesting() {
  tab_strip_editable_for_testing_ = false;
}

bool VerticalTabStripRegionView::IsTabStripCloseable() const {
  if (!drag_handler_) {
    return true;
  }
  if (auto* drag_controller =
          drag_handler_->GetDragContext()->GetDragController()) {
    return drag_controller->IsMovingLastTab();
  }
  return true;
}

void VerticalTabStripRegionView::UpdateLoadingAnimations(
    const base::TimeDelta& elapsed_time) {
  for (tabs::TabInterface* tab : *tab_strip_model_) {
    const TabCollectionNode* node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    VerticalTabView* tab_view =
        views::AsViewClass<VerticalTabView>(node->view());
    CHECK(tab_view);
    tab_view->StepLoadingAnimation(elapsed_time);
  }
}

std::optional<int> VerticalTabStripRegionView::GetFocusedTabIndex() const {
  const views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return std::nullopt;
  }

  const views::View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view) {
    return std::nullopt;
  }

  for (int i = 0; i < tab_strip_model_->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(i);
    const TabCollectionNode* node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    if (node && node->view() == focused_view) {
      return i;
    }
  }

  return std::nullopt;
}

const TabRendererData& VerticalTabStripRegionView::GetTabRendererData(
    int tab_index) {
  tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);
  CHECK(tab);

  const TabCollectionNode* node =
      root_node_->GetNodeForHandle(tab->GetHandle());
  CHECK(node);

  VerticalTabView* tab_view = views::AsViewClass<VerticalTabView>(node->view());
  CHECK(tab_view);

  return tab_view->data();
}

views::View* VerticalTabStripRegionView::GetTabAnchorViewAt(int tab_index) {
  tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);
  CHECK(tab) << "No tab found for tab_index: " << tab_index;

  const TabCollectionNode* node =
      root_node_->GetNodeForHandle(tab->GetHandle());
  CHECK(node) << "No node found for tab handle";

  return node->view();
}

views::View* VerticalTabStripRegionView::GetTabGroupAnchorView(
    const tab_groups::TabGroupId& group) {
  if (!tab_strip_model_->SupportsTabGroups()) {
    return nullptr;
  }

  if (const TabGroup* tab_group =
          tab_strip_model_->group_model()->GetTabGroup(group)) {
    return root_node_->GetNodeForHandle(tab_group->GetCollectionHandle())
        ->view();
  }

  return nullptr;
}

void VerticalTabStripRegionView::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  top_button_container_->GetUnfocusButton()->SetVisible(
      new_focused_group_id.has_value());
  // Temporarily, we are updating the visibility of the collapse action to be
  // inverse to the unfocus button because of horizontal space constraints in
  // the top container.
  actions::ActionItem* collapse_action =
      actions::ActionManager::Get().FindAction(kActionToggleCollapseVertical,
                                               root_action_item_);
  collapse_action->SetVisible(!new_focused_group_id.has_value());
}

TabDragContext* VerticalTabStripRegionView::GetDragContext() {
  return drag_handler_->GetDragContext();
}

BrowserRootView::DropTarget* VerticalTabStripRegionView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  if (tab_strip_view_) {
    if (tab_strip_view_->bounds().Contains(loc_in_local_coords)) {
      return this;
    }
  }
  return nullptr;
}

views::View* VerticalTabStripRegionView::GetViewForDrop() {
  return this;
}

std::optional<BrowserRootView::DropIndex>
VerticalTabStripRegionView::GetDropIndex(const ui::DropTargetEvent& event) {
  // Check pinned tabs.
  VerticalPinnedTabContainerView* pinned_container = GetPinnedTabsContainer();
  if (pinned_container) {
    gfx::Point loc_in_pinned = views::View::ConvertPointToTarget(
        this, pinned_container, event.location());
    if (loc_in_pinned.y() >= 0 &&
        loc_in_pinned.y() < pinned_container->height()) {
      return pinned_container->GetLinkDropIndex(loc_in_pinned);
    }
  }

  // Check unpinned tabs.
  VerticalUnpinnedTabContainerView* unpinned_container =
      GetUnpinnedTabsContainer();
  if (unpinned_container) {
    gfx::Point loc_in_unpinned = views::View::ConvertPointToTarget(
        this, unpinned_container, event.location());
    if (loc_in_unpinned.y() >= 0 &&
        loc_in_unpinned.y() < unpinned_container->height()) {
      return unpinned_container->GetLinkDropIndex(loc_in_unpinned);
    }
  }

  // If it's between containers or at the end, return the end of the unpinned
  // container.
  if (unpinned_container) {
    return unpinned_container->GetLinkDropIndex(
        gfx::Point(0, unpinned_container->height()));
  }

  return std::nullopt;
}

void VerticalTabStripRegionView::SetTabStripObserver(
    TabStripObserver* observer) {
  // Do nothing.
}

views::View* VerticalTabStripRegionView::GetTabStripView() {
  return tab_strip_view_;
}

bool VerticalTabStripRegionView::TraverseUsingUpDownKeys() {
  return true;
}

void VerticalTabStripRegionView::OnResize(int resize_amount,
                                          bool done_resizing) {
  if (!starting_width_on_resize_.has_value()) {
    starting_width_on_resize_ = width();
  }
  const int proposed_width = starting_width_on_resize_.value() + resize_amount;
  if (done_resizing) {
    starting_width_on_resize_ = std::nullopt;
  }

  tabs::VerticalTabStripState new_state;
  if (proposed_width > kCollapseSnapWidth) {
    new_state.collapsed = false;
    new_state.uncollapsed_width =
        std::clamp(proposed_width, kUncollapsedMinWidth, kUncollapsedMaxWidth);
    if (done_resizing) {
      // We only want to save the uncollapsed width to the state controller if
      // the user has lifted their mouse, otherwise dragging the resize area to
      // collapse will cause a subsequent collapse button click to only expand
      // to the minimum expanded width, and not to the starting width of the
      // drag-to-collapse operation.
      state_controller_->SetUncollapsedWidth(new_state.uncollapsed_width);
    }
  } else {
    new_state.collapsed = true;
    new_state.uncollapsed_width = target_collapse_state_.uncollapsed_width;
  }

  if (done_resizing) {
    base::RecordAction(base::UserMetricsAction(
        new_state.collapsed ? "VerticalTabs_TabStrip_ResizeToCollapsed"
                            : "VerticalTabs_TabStrip_ResizeToUncollapsed"));
    base::UmaHistogramCounts1000(
        "Tabs.VerticalTabs.TabStripSize",
        new_state.collapsed ? kCollapsedWidth : new_state.uncollapsed_width);
  }

  UpdateCollapseState(new_state);
}

void VerticalTabStripRegionView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &resize_animation_);
  InvalidateLayout();
}

bool VerticalTabStripRegionView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  // Check the resize area first, it should always take precedence over other
  // children regardless of order.
  if (IsHitInView(resize_area_, point)) {
    return false;
  }

  if (IsHitInView(top_button_container_, point)) {
    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, top_button_container_,
                                      &point_in_child);
    return top_button_container_->IsPositionInWindowCaption(point_in_child);
  }

  if (IsHitInView(bottom_button_container_, point)) {
    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, bottom_button_container_,
                                      &point_in_child);
    return bottom_button_container_->IsPositionInWindowCaption(point_in_child);
  }

  // For any of the other children, absorb the click as non window caption.
  for (views::View* child : children()) {
    if (!child->GetVisible()) {
      continue;
    }

    gfx::Point point_in_child = point;
    views::View::ConvertPointToTarget(this, child, &point_in_child);
    if (child->HitTestPoint(point_in_child)) {
      return false;
    }
  }

  // If the click doesnt fall under any view,then it counts as window caption.
  return true;
}

void VerticalTabStripRegionView::SetToolbarHeightForLayout(int toolbar_height) {
  top_button_container_->SetToolbarHeightForLayout(toolbar_height);
}

void VerticalTabStripRegionView::SetCaptionButtonWidthForLayout(
    int caption_button_width) {
  top_button_container_->SetCaptionButtonWidthForLayout(caption_button_width);
}

VerticalPinnedTabContainerView*
VerticalTabStripRegionView::GetPinnedTabsContainer() {
  return tab_strip_view_->GetPinnedTabsContainer();
}

VerticalUnpinnedTabContainerView*
VerticalTabStripRegionView::GetUnpinnedTabsContainer() {
  return tab_strip_view_->GetUnpinnedTabsContainer();
}

views::View* VerticalTabStripRegionView::SetTabStripView(
    std::unique_ptr<views::View> view) {
  CHECK(views::IsViewClass<VerticalTabStripView>(view.get()));
  tab_strip_view_ =
      static_cast<VerticalTabStripView*>(AddChildView(std::move(view)));
  OnCollapsedStateChanged(state_controller_);
  tab_strip_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  tab_strip_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding),
          0));
  tab_strip_view_->InitializeTabStrip(*tab_strip_model_);

  std::optional<size_t> separator_index = GetIndexOf(top_button_separator_);
  CHECK(separator_index.has_value());
  ReorderChildView(tab_strip_view_, separator_index.value() + 1);
  return tab_strip_view_;
}

void VerticalTabStripRegionView::ClearTabStripView(views::View* view) {
  CHECK(tab_strip_view_);
  CHECK(tab_strip_view_ == view);
  RemoveChildViewT(std::exchange(tab_strip_view_, nullptr));
}

void VerticalTabStripRegionView::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* state_controller) {
  if (target_collapse_state_.collapsed != state_controller->IsCollapsed()) {
    // UpdateCollapseState is responsible for setting the collapsed state of the
    // state controller due to a resizing operation. To avoid reentrancy in that
    // case, only update the collapse state if the state controller's collapse
    // state is different than the current target collapse state, which can
    // happen due to the collapse button being pressed.
    UpdateCollapseState(state_controller_->GetState());
  }

  const int padding = GetLayoutConstant(
      state_controller_->IsCollapsed()
          ? LayoutConstant::kVerticalTabStripCollapsedPadding
          : LayoutConstant::kVerticalTabStripUncollapsedPadding);
  // The TopContainer handles the padding distance to the separator so that we
  // can control how far it is in the various states.
  int separator_padding = padding;
  if (state_controller_->IsCollapsed()) {
    const int collapsed_separator_width = GetLayoutConstant(
        LayoutConstant::kVerticalTabStripCollapsedSeparatorWidth);
    separator_padding = (kCollapsedWidth - collapsed_separator_width) / 2;
  }
  top_button_separator_->SetProperty(views::kMarginsKey,
                                     gfx::Insets::VH(0, separator_padding));
  if (state_controller->IsCollapsed()) {
    // If the VT Strip is collapsed, then we need exactly |padding| on the top,
    // left, and right.
    top_button_container_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(padding, padding, kRegionVerticalPadding, padding));
  } else {
    // If the VT Strip is not collapsed, then we want to align the heights of
    // the TopContainer w/ the the height of the toolbar. Both of these
    // components start at the top of the window. We keep no vertical padding so
    // that the separator can lay adjacent to it.
    top_button_container_->SetProperty(views::kMarginsKey,
                                       gfx::Insets::VH(0, padding));
  }

  bottom_button_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding),
          padding, 0, padding));

  resize_area_width_ = state_controller->IsCollapsed()
                           ? kCollapsedResizeAreaWidth
                           : kResizeAreaWidth;

  flex_layout_->SetInteriorMargin(gfx::Insets::TLBR(
      0, 0,
      GetLayoutConstant(LayoutConstant::kVerticalTabStripUncollapsedPadding),
      0));

  if (tab_strip_view_) {
    tab_strip_view_->SetCollapsedState(state_controller->IsCollapsed());
  }
}

void VerticalTabStripRegionView::UpdateCollapseState(
    tabs::VerticalTabStripState new_state) {
  bool previously_collapsed = target_collapse_state_.collapsed;
  target_collapse_state_ = new_state;
  if (previously_collapsed != target_collapse_state_.collapsed) {
    if (target_collapse_state_.collapsed) {
      resize_animation_.Hide();
    } else {
      resize_animation_.Show();
    }
    state_controller_->SetCollapsed(target_collapse_state_.collapsed);
  } else if (!target_collapse_state_.collapsed &&
             !resize_animation_.is_animating()) {
    // If we are still in the expanding animation, invalidating the layout will
    // happen in AnimationProgressed, instead of here.
    InvalidateLayout();
  }
}

void VerticalTabStripRegionView::UpdateColors() {
  top_button_separator_->SetColorId(IsFrameActive()
                                        ? kColorTabDividerFrameActive
                                        : kColorTabDividerFrameInactive);
}

bool VerticalTabStripRegionView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

gfx::Rect VerticalTabStripRegionView::GetTabStripDraggableBounds() const {
  // Tabs should be draggable from the top of the tab strip to the bottom of the
  // tab strip's max size, saving space for the bottom button container and
  // padding.
  gfx::Rect tab_strip_draggable_bounds = tab_strip_view_->GetBoundsInScreen();
  tab_strip_draggable_bounds.set_height(
      GetBoundsInScreen().bottom() -
      bottom_button_container_->GetMinimumSize().height() -
      flex_layout_->interior_margin().height() -
      tab_strip_draggable_bounds.y());
  return tab_strip_draggable_bounds;
}

void VerticalTabStripRegionView::RecordNewTabButtonPressed() {
  new_tab_button_pressed_start_time_ = base::TimeTicks::Now();

  base::RecordAction(base::UserMetricsAction("NewTab_Button"));
}

void VerticalTabStripRegionView::OnChildrenAdded() {
  if (new_tab_button_pressed_start_time_.has_value()) {
    base::UmaHistogramTimes(
        "TabStrip.TimeToCreateNewTabFromPress",
        base::TimeTicks::Now() - new_tab_button_pressed_start_time_.value());
    new_tab_button_pressed_start_time_.reset();
  }
}

TabDragTarget* VerticalTabStripRegionView::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  if (!drag_handler_) {
    return nullptr;
  }
  gfx::Rect tab_strip_draggable_bounds = GetTabStripDraggableBounds();
  if (!tab_strip_draggable_bounds.Contains(point_in_screen)) {
    return nullptr;
  }

  // Note: if the drag has not attached to this tab strip yet, it doesn't matter
  // which container is used because the first drag loop iteration just attaches
  // it.
  if (drag_handler_->IsDraggingPinnedTabs()) {
    return &GetPinnedTabsContainer()->GetTabDragTarget(point_in_screen);
  }
  return &GetUnpinnedTabsContainer()->GetTabDragTarget(point_in_screen);
}

bool VerticalTabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->GetDropFormats(formats,
                                                           format_types);
  }
  return false;
}

bool VerticalTabStripRegionView::CanDrop(const OSExchangeData& data) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->CanDrop(data);
  }
  return false;
}

void VerticalTabStripRegionView::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  drag_handler_->GetDragContext()->OnDragEntered(event);
}

int VerticalTabStripRegionView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  return drag_handler_->GetDragContext()->OnDragUpdated(event);
}

void VerticalTabStripRegionView::OnDragExited() {
  CHECK(drag_handler_ && drag_handler_->GetDragContext());
  drag_handler_->GetDragContext()->OnDragExited();
}

BEGIN_METADATA(VerticalTabStripRegionView)
END_METADATA
