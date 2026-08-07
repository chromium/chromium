// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view_layout.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupLineCornerRadius = 4;

const TabGroup* GetTabGroupFromNode(TabCollectionNode* node) {
  CHECK(node);
  return static_cast<const tabs::TabGroupTabCollection*>(
             std::get<const tabs::TabCollection*>(node->GetNodeData()))
      ->GetTabGroup();
}
}  // namespace

TabGroupView::TabGroupView(TabCollectionNode* collection_node)
    : DraggedTabsContainer(
          static_cast<views::View&>(*this),
          collection_node,
          collection_node && collection_node->orientation() ==
                                 TabStripOrientation::kHorizontal
              ? DragAxes::kHorizontalOnly
              : DragAxes::kVerticalOnly,
          collection_node && collection_node->orientation() ==
                                 TabStripOrientation::kHorizontal
              ? DragLayout::kHorizontal
              : DragLayout::kVertical),
      collection_node_(collection_node),
      tab_group_visual_data_(
          *GetTabGroupFromNode(collection_node_)->visual_data()),
      group_header_(AddChildView(std::make_unique<TabGroupHeaderView>(
          *this,
          collection_node_->orientation(),
          collection_node_->GetController()->GetStateController(),
          &tab_group_visual_data_))),
      group_line_(AddChildView(std::make_unique<views::View>())),
      layout_manager_(*SetLayoutManager(
          std::make_unique<TabCollectionAnimatingLayoutManager>(
              std::make_unique<TabGroupViewLayout>(
                  collection_node->orientation()),
              *this))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabGroupView::AttachChildView, base::Unretained(this)));
  collection_node->set_detach_child_from_node(base::BindRepeating(
      &TabGroupView::DetachChildView, base::Unretained(this)));

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &TabGroupView::ResetCollectionNode, base::Unretained(this)));

  TabGroup* const tab_group =
      const_cast<TabGroup*>(GetTabGroupFromNode(collection_node_));

  tab_group_data_observer_ =
      std::make_unique<tabs::TabGroupDataObserver>(tab_group);
  tab_group_data_changed_subscription_ =
      tab_group_data_observer_->RegisterTabGroupDataChangedCallback(
          base::BindRepeating(&TabGroupView::OnDataChanged,
                              base::Unretained(this)));
  OnDataChanged();
}

TabGroupView::~TabGroupView() = default;

void TabGroupView::OnThemeChanged() {
  views::View::OnThemeChanged();
  OnDataChanged();
}

void TabGroupView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureLongTap) {
    ui::GestureEvent converted_event(*event, static_cast<views::View*>(this),
                                     static_cast<views::View*>(group_header_));
    group_header_->OnGestureEvent(&converted_event);
    event->SetHandled();
  }
}

void TabGroupView::ToggleCollapsedState(
    ToggleTabGroupCollapsedStateOrigin origin) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return;
  }

  collection_node_->GetController()->ToggleTabGroupCollapsedState(
      GetTabGroupFromNode(collection_node_), origin);
  InvalidateLayout();
}

std::unique_ptr<views::Widget> TabGroupView::ShowGroupEditorBubble(
    bool stop_context_menu_propagation) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return nullptr;
  }

  // When the tab strip is collapsed, anchor to the group header, otherwise
  // anchor to the editor bubble button.
  views::View* anchor_view =
      (GetTabStripCollapseState() !=
           tabs::VerticalTabStripCollapseState::kExpanded ||
       !group_header_->editor_bubble_button())
          ? views::AsViewClass<views::View>(group_header_)
          : views::AsViewClass<views::View>(
                group_header_->editor_bubble_button());
  return collection_node_->GetController()->ShowGroupEditorBubble(
      GetTabGroupFromNode(collection_node_)->id(), anchor_view,
      stop_context_menu_propagation);
}

bool TabGroupView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsDragging();
}

bool TabGroupView::IsViewDragging(const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool TabGroupView::ShouldAnimateOpacityForAddAndRemove(
    const views::View& child_view) const {
  // Only animate opacity for tab views.
  return views::IsViewClass<TabView>(&child_view);
}

bool TabGroupView::ShouldSnapToTarget(const views::View& child_view) const {
  return views::IsViewClass<SplitTabView>(&child_view);
}

void TabGroupView::OnAnimationEnded() {
  // For collapsed tab groups update child visibility only once animations have
  // completed. This allows tabs to remain visible as the group animates closed.
  if (tab_group_visual_data_.is_collapsed()) {
    UpdateChildVisibilityForCollapseState(true);
  }
}

std::u16string TabGroupView::GetGroupContentString() const {
  if (!collection_node_) {
    return std::u16string();
  }

  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  if (group->tab_count() == 0) {
    return std::u16string();
  }

  return tab_groups::GetGroupContentString(group);
}

bool TabGroupView::IsValid() const {
  return collection_node_;
}

void TabGroupView::AttachChildView(std::unique_ptr<views::View> child_view,
                                   const gfx::Rect& previous_bounds_in_screen) {
  if (IsCollapsed()) {
    // When child views are added to a group when in collapsed state,
    // expand it to reveal the newly added views.
    ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  }
  layout_manager_->AnimateAndReparentView(std::move(child_view),
                                          previous_bounds_in_screen);
}

std::unique_ptr<views::View> TabGroupView::DetachChildView(
    views::View* child_view) {
  if (IsCollapsed()) {
    // The child views are invisible in collapsed state. When child views
    // are detached from the group while collapsed, reset its visibility.
    child_view->SetVisible(true);
  }
  return RemoveChildViewT(child_view);
}

void TabGroupView::ResetCollectionNode() {
  HideHoverCard(TabSlotController::HoverCardUpdateType::kTabRemoved);
  node_destroyed_subscription_ = {};
  tab_group_data_changed_subscription_ = {};
  tab_group_data_observer_.reset();
  collection_node_ = nullptr;
}

void TabGroupView::OnDataChanged() {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return;
  }

  const tabs::TabGroupData& tab_group_data =
      tab_group_data_observer_->tab_group_data();
  tab_group_visual_data_ = tab_group_data.visual_data;
  group_header_->OnDataChanged(tab_group_data);

  // If the tab group is not collapsed update child visibility immediately. This
  // allows tabs to be visible as they are animated in.
  if (!tab_group_visual_data_.is_collapsed()) {
    UpdateChildVisibilityForCollapseState(false);
  }

  if (GetColorProvider()) {
    SkColor color = GetColorProvider()->GetColor(GetTabGroupTabStripColorId(
        tab_group_visual_data_.color(), GetWidget()->ShouldPaintAsActive()));
    group_line_->SetBackground(views::CreateRoundedRectBackground(
        color, gfx::RoundedCornersF(0, kGroupLineCornerRadius,
                                    kGroupLineCornerRadius, 0)));
  }

  InvalidateLayout();
}

void TabGroupView::SetIsCollapsed(bool is_collapsed) {
  if (is_collapsed_ == is_collapsed) {
    return;
  }
  is_collapsed_ = is_collapsed;
  InvalidateLayout();
}

void TabGroupView::UpdateChildVisibilityForCollapseState(bool collapsed) {
  // Collection node may not exist at this point during browser shutdown.
  if (!collection_node_) {
    return;
  }
  SetIsCollapsed(collapsed);
  for (auto* child : collection_node_->GetDirectChildren()) {
    child->SetVisible(!collapsed);
  }
}

bool TabGroupView::IsCollapsed() const {
  return tab_group_visual_data_.is_collapsed();
}

views::ScrollView* TabGroupView::GetScrollViewForContainer() const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<views::View*>(parent()));
}

void TabGroupView::UpdateTargetLayoutForDrag(
    const std::vector<const views::View*>& views_to_snap) {
  layout_manager_->ResetViewsToTargetLayout(views_to_snap);
}

const views::ProposedLayout& TabGroupView::GetLayoutForDrag() const {
  return layout_manager_->target_layout();
}

const TabCollectionNode* TabGroupView::GetCollectionNodeFromView(
    const views::View& view) const {
  if (auto* tab_view = views::AsViewClass<TabView>(&view)) {
    return tab_view->collection_node();
  } else if (auto* split_tab_view = views::AsViewClass<SplitTabView>(&view)) {
    return split_tab_view->collection_node();
  }
  return nullptr;
}

std::optional<BrowserRootView::DropIndex> TabGroupView::GetLinkDropIndex(
    const gfx::Point& loc_in_group) {
  if (!collection_node_) {
    return std::nullopt;
  }
  const bool is_horizontal =
      collection_node_->orientation() == TabStripOrientation::kHorizontal;

  // Use the position along drag axis to find the child view being dragged over.
  const int header_end = is_horizontal ? group_header_->bounds().right()
                                       : group_header_->bounds().bottom();
  const int loc_coord = is_horizontal ? loc_in_group.x() : loc_in_group.y();
  const int header_center = is_horizontal
                                ? group_header_->bounds().CenterPoint().x()
                                : group_header_->bounds().CenterPoint().y();

  if (loc_coord < header_end) {
    // Determine whether the drop is on the leading or trailing half of the
    // header.
    const bool is_leading = loc_coord < header_center;
    return GetDragHandler().GetLinkDropIndexForNode(
        *collection_node_, is_leading
                               ? std::make_optional(DragPositionHint::kBefore)
                               : std::nullopt);
  }

  for (const auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    const int view_end =
        is_horizontal ? view->bounds().right() : view->bounds().bottom();
    if (loc_coord > view_end) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_group);

    // If the drag is over the margins from the edges of the tab, then
    // consider this drag as a before/after rather than over.
    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;
    const int child_coord = is_horizontal ? loc_in_child.x() : loc_in_child.y();
    const int child_size = is_horizontal ? view->width() : view->height();
    if (child_coord < child_size * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (child_coord > child_size * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide which
      // tab to replace.
      auto* split_view = views::AsViewClass<SplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_group);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  // Fallback to the end of the group.
  return GetDragHandler().GetLinkDropIndexForNode(*collection_node_,
                                                  DragPositionHint::kAfter);
}

void TabGroupView::InitHeaderDrag(const ui::LocatedEvent& event) {
  CHECK(collection_node_);
  const ui::ListSelectionModel original_selection_model =
      collection_node_->GetController()->GetSelectionModel();
  GetDragHandler().InitializeDrag(*collection_node_, original_selection_model,
                                  event);
}

bool TabGroupView::ContinueHeaderDrag(const ui::LocatedEvent& event) {
  return GetDragHandler().ContinueDrag(*group_header_, event);
}

void TabGroupView::CancelHeaderDrag() {
  GetDragHandler().EndDrag(EndDragReason::kCancel);
}

const TabGroup& TabGroupView::GetTabGroup() const {
  CHECK(collection_node_);
  return *GetTabGroupFromNode(collection_node_);
}

const tabs::TabGroupData& TabGroupView::GetTabGroupData() const {
  CHECK(tab_group_data_observer_);
  return tab_group_data_observer_->tab_group_data();
}

void TabGroupView::UpdateHoverCard(int update_type) const {
  if (!collection_node_ || !group_header_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        group_header_,
        static_cast<TabSlotController::HoverCardUpdateType>(update_type));
  }
}

void TabGroupView::HideHoverCard(int update_type) const {
  if (!collection_node_) {
    return;
  }

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        nullptr,
        static_cast<TabSlotController::HoverCardUpdateType>(update_type));
  }
}

bool TabGroupView::IsFocusInTabStrip() {
  auto* tab_strip_view = GetTabStripView(this);
  return tab_strip_view && tab_strip_view->IsFocusInTabStrip();
}

std::unique_ptr<ExpandOnHoverLock> TabGroupView::AcquireExpandOnHoverLock() {
  if (!collection_node_ || !collection_node_->GetController()) {
    return nullptr;
  }

  BrowserView* browser_view =
      collection_node_->GetController()->GetBrowserView();
  CHECK(browser_view);
  CHECK(browser_view->tab_strip_view());
  return browser_view->tab_strip_view()->GetExpandOnHoverLock(
      ExpandOnHoverLockType::kKeepCurrentState);
}

void TabGroupView::ShiftGroupUp() {
  if (!collection_node_) {
    return;
  }
  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  collection_node_->GetController()->ShiftGroupUp(group->id());
}

void TabGroupView::ShiftGroupDown() {
  if (!collection_node_) {
    return;
  }
  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  collection_node_->GetController()->ShiftGroupDown(group->id());
}

bool TabGroupView::IsGroupFocused() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return collection_node_->GetController()->GetFocusedGroup() ==
         GetTabGroup().id();
}

BEGIN_METADATA(TabGroupView)
END_METADATA
