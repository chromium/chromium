// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include <algorithm>
#include <variant>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
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
constexpr gfx::Insets kRegionInteriorMargins = gfx::Insets::VH(8, 0);

constexpr int kRegionVerticalPadding = 5;
}  // namespace

VerticalTabStripRegionView::VerticalTabStripRegionView(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : tab_strip_model_(browser->GetTabStripModel()),
      state_controller_(state_controller),
      resize_animation_(this) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kRegionInteriorMargins)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(
                      kRegionVerticalPadding,
                      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING)))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  // Create child views.
  top_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripTopContainer>(
          state_controller_, root_action_item));

  top_button_separator_ = AddChildView(std::make_unique<views::Separator>());

  bottom_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripBottomContainer>(
          state_controller_, root_action_item, browser));

  gemini_button_ = AddChildView(std::make_unique<views::View>());

  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));
  resize_area_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  resize_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(base::Milliseconds(450)));
  resize_animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);
  resize_animation_.Reset(!state_controller_->IsCollapsed());

  target_collapse_state_ = state_controller_->GetState();
  SetPreferredSize(gfx::Size(target_collapse_state_.collapsed
                                 ? kCollapsedWidth
                                 : target_collapse_state_.uncollapsed_width,
                             0));
  OnCollapsedStateChanged(state_controller_);
  collapsed_state_changed_subscription_ =
      state_controller_->RegisterOnStateChanged(base::BindRepeating(
          &VerticalTabStripRegionView::OnCollapsedStateChanged,
          base::Unretained(this)));

  SetProperty(views::kElementIdentifierKey, kVerticalTabStripRegionElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);

  root_node_ = std::make_unique<RootTabCollectionNode>(
      browser->GetTabStripModel(),
      base::BindRepeating(&VerticalTabStripRegionView::SetTabStripView,
                          base::Unretained(this)));

  UpdateBackgroundColors();
}

VerticalTabStripRegionView::~VerticalTabStripRegionView() {
  root_node_->SetController(nullptr);
  tab_strip_controller_.reset();
  auto handler = RemoveChildViewT(drag_handler_);
  drag_handler_ = nullptr;
}

void VerticalTabStripRegionView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabStripRegionView::UpdateBackgroundColors,
          base::Unretained(this)));
}

void VerticalTabStripRegionView::Layout(PassKey) {
  LayoutSuperclass<views::AccessiblePaneView>(this);

  // Manually position the resize area as it overlaps views handled by the flex
  // layout.
  resize_area_->SetBoundsRect(gfx::Rect(bounds().right() - kResizeAreaWidth, 0,
                                        kResizeAreaWidth, bounds().height()));
}

views::View* VerticalTabStripRegionView::GetDefaultFocusableChild() {
  return top_button_container_;
}

bool VerticalTabStripRegionView::IsTabStripEditable() const {
  // TODO(crbug.com/467710547): This needs to consider the drag context. Wait
  // until that is implemented before updating this function.
  NOTIMPLEMENTED();
  return tab_strip_editable_for_testing_;
}

void VerticalTabStripRegionView::DisableTabStripEditingForTesting() const {
  // TODO(crbug.com/467710617): Implement this in VerticalTabStripView.
  NOTIMPLEMENTED();
}

bool VerticalTabStripRegionView::IsTabStripCloseable() const {
  // TODO(crbug.com/467710547): Return TabDragContext::IsTabStripCloseable once
  // it exists.
  NOTIMPLEMENTED();
  return true;
}

bool VerticalTabStripRegionView::IsAnimating() const {
  // TODO(crbug.com/467710547): Return if the view or drag context is animating
  // something.
  NOTIMPLEMENTED();
  return true;
}

void VerticalTabStripRegionView::StopAnimating() {
  // TODO(crbug.com/467710547): Stop any ongoing animation in the
  // VerticalTabStripView.
  NOTIMPLEMENTED();
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

TabDragContext* VerticalTabStripRegionView::GetDragContext() {
  return drag_handler_.get();
}
void VerticalTabStripRegionView::SetTabStripObserver(
    TabStripObserver* observer) {
  // Do nothing.
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

  UpdateCollapseState(new_state);
}

void VerticalTabStripRegionView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &resize_animation_);
  double width = kCollapsedWidth +
                 (target_collapse_state_.uncollapsed_width - kCollapsedWidth) *
                     resize_animation_.GetCurrentValue();
  ResizeToWidth((resize_animation_.IsShowing() ? std::floor<double>
                                               : std::ceil<double>)(width));
}

bool VerticalTabStripRegionView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  gfx::Point point_in_target = point;
  views::View::ConvertPointToTarget(this, top_button_container_,
                                    &point_in_target);
  if (top_button_container_->HitTestPoint(point_in_target)) {
    return top_button_container_->IsPositionInWindowCaption(point_in_target);
  }
  return false;
}

void VerticalTabStripRegionView::CreateTabStripController(
    BrowserView* browser_view) {
  std::unique_ptr<TabMenuModelFactory> tab_menu_model_factory;
  if (browser_view && browser_view->browser()->app_controller()) {
    tab_menu_model_factory =
        browser_view->browser()->app_controller()->GetTabMenuModelFactory();
  }

  TabStripModel* tab_strip_model = browser_view->browser()->GetTabStripModel();
  CHECK(tab_strip_model);
  auto drag_handler = std::make_unique<VerticalTabDragHandlerImpl>(
      *tab_strip_model, *root_node_.get());
  drag_handler_ = drag_handler.get();

  tab_strip_controller_ = std::make_unique<VerticalTabStripController>(
      tab_strip_model, browser_view, *AddChildView(std::move(drag_handler)),
      std::move(tab_menu_model_factory));

  if (root_node_) {
    root_node_->SetController(tab_strip_controller_.get());
  }
}

void VerticalTabStripRegionView::SetToolbarHeightForLayout(
    const int toolbar_height) {
  top_button_container_->SetToolbarHeightForLayout(toolbar_height);
}

void VerticalTabStripRegionView::SetExclusionWidthForLayout(
    const int exclusion_width) {
  exclusion_width_ = exclusion_width;
  top_button_container_->SetExclusionWidthForLayout(exclusion_width);
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
  tab_strip_view_->SetCollapsedState(state_controller_->IsCollapsed());
  gfx::Insets tab_container_margins = gfx::Insets::TLBR(
      kRegionVerticalPadding,
      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING),
      kRegionVerticalPadding, 0);
  tab_strip_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  tab_strip_view_->SetProperty(views::kMarginsKey, tab_container_margins);
  std::optional<size_t> separator_index = GetIndexOf(top_button_separator_);
  CHECK(separator_index.has_value());
  ReorderChildView(tab_strip_view_, separator_index.value() + 1);
  return tab_strip_view_;
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
  if (tab_strip_view_) {
    tab_strip_view_->SetCollapsedState(state_controller->IsCollapsed());
  }
  bottom_button_container_->OnCollapsedStateChanged(state_controller);
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
  } else if (!target_collapse_state_.collapsed &&
             !resize_animation_.is_animating()) {
    // If we are still in the expanding animation, resizing to the updated
    // uncollapsed width will happen in AnimationProgressed, instead of here.
    ResizeToWidth(target_collapse_state_.uncollapsed_width);
  }
}

void VerticalTabStripRegionView::ResizeToWidth(int width) {
  // The collapsed state of the state controller is used to determine whether
  // the tab strip includes the exclusion zone or is drawn underneath it. So
  // instead of setting it immediately upon starting the resize animation, only
  // do so once it crosses the exclusion width threshold.
  if (!exclusion_width_.has_value() ||
      target_collapse_state_.collapsed == width < exclusion_width_.value()) {
    state_controller_->SetCollapsed(target_collapse_state_.collapsed);
  }

  // BrowserViewLayout uses the preferred size's width. The height is not used.
  SetPreferredSize(gfx::Size(width, 0));
}

void VerticalTabStripRegionView::UpdateBackgroundColors() {
  SetBackground(views::CreateSolidBackground(
      IsFrameActive() ? ui::kColorFrameActive : ui::kColorFrameInactive));
  top_button_separator_->SetColorId(IsFrameActive()
                                        ? kColorTabDividerFrameActive
                                        : kColorTabDividerFrameInactive);
}

bool VerticalTabStripRegionView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

TabDragTarget* VerticalTabStripRegionView::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  VerticalUnpinnedTabContainerView* container = GetUnpinnedTabsContainer();
  CHECK(container);
  if (container->GetBoundsInScreen().Contains(point_in_screen)) {
    return container;
  }
  return nullptr;
}

BEGIN_METADATA(VerticalTabStripRegionView)
END_METADATA
