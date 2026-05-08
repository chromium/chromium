// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include <numeric>

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kTabVerticalPadding = 2;
constexpr int kGroupLineWidth = 2;
constexpr int kGroupLineCollapsedLeadingPadding = 6;
constexpr int kGroupLineCornerRadius = 4;
constexpr int kGroupHeaderHeight = 26;
constexpr int kGroupHeaderVerticalMargin = 4;

const TabGroup* GetTabGroupFromNode(TabCollectionNode* node) {
  CHECK(node);
  return static_cast<const tabs::TabGroupTabCollection*>(
             std::get<const tabs::TabCollection*>(node->GetNodeData()))
      ->GetTabGroup();
}
}  // namespace

VerticalTabGroupView::VerticalTabGroupView(TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this),
                                   collection_node,
                                   DragAxes::kVerticalOnly,
                                   DragLayout::kVertical),
      collection_node_(collection_node),
      tab_group_visual_data_(
          *GetTabGroupFromNode(collection_node_)->visual_data()),
      group_header_(AddChildView(std::make_unique<VerticalTabGroupHeaderView>(
          *this,
          collection_node_->GetController()->GetStateController(),
          &tab_group_visual_data_))),
      group_line_(AddChildView(std::make_unique<views::View>())),
      layout_manager_(*SetLayoutManager(
          std::make_unique<TabCollectionAnimatingLayoutManager>(
              std::make_unique<views::DelegatingLayoutManager>(this),
              *this))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &VerticalTabGroupView::AttachChildView, base::Unretained(this)));
  collection_node->set_detach_child_from_node(base::BindRepeating(
      &VerticalTabGroupView::DetachChildView, base::Unretained(this)));

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabGroupView::ResetCollectionNode, base::Unretained(this)));

  TabGroup* const tab_group =
      const_cast<TabGroup*>(GetTabGroupFromNode(collection_node_));

  tab_group_data_observer_ =
      std::make_unique<tabs::TabGroupDataObserver>(tab_group);
  tab_group_data_changed_subscription_ =
      tab_group_data_observer_->RegisterTabGroupDataChangedCallback(
          base::BindRepeating(&VerticalTabGroupView::OnDataChanged,
                              base::Unretained(this)));
  OnDataChanged();
}

VerticalTabGroupView::~VerticalTabGroupView() = default;

void VerticalTabGroupView::OnThemeChanged() {
  views::View::OnThemeChanged();
  OnDataChanged();
}

void VerticalTabGroupView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureLongTap) {
    ui::GestureEvent converted_event(*event, static_cast<views::View*>(this),
                                     static_cast<views::View*>(group_header_));
    group_header_->OnGestureEvent(&converted_event);
    event->SetHandled();
  }
}

views::ProposedLayout VerticalTabGroupView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = kGroupHeaderVerticalMargin;
  auto tab_strip_collapse_state = GetTabStripCollapseState();

  gfx::Rect header_bounds;
  gfx::Rect group_line_bounds;
  group_line_bounds.set_width(kGroupLineWidth);

  // If the tab strip is collapsed then the group line should appear on the
  // leading side of all grouped tabs and the header.
  if (tab_strip_collapse_state !=
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(kGroupLineCollapsedLeadingPadding);
    group_line_bounds.set_y(height);
    header_bounds.set_x(
        GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding));
  }

  header_bounds.set_y(height);
  header_bounds.set_height(kGroupHeaderHeight);
  // If width is bounded, the group header should respect the width constraints
  // and take up the available width excluding trailing horizontal padding.
  if (size_bounds.width().is_bounded()) {
    header_bounds.set_width(size_bounds.width().value() - header_bounds.x());
  }
  layouts.child_layouts.emplace_back(
      group_header_.get(), group_header_->GetVisible(), header_bounds);
  height +=
      header_bounds.height() + kGroupHeaderVerticalMargin + kTabVerticalPadding;
  width = std::max(width, header_bounds.width());

  // If the tab strip is not collapsed then the group line is below and left
  // aligned with the header.
  if (tab_strip_collapse_state ==
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(
        (VerticalTabGroupView::kTabLeadingPadding - kGroupLineWidth) / 2);
    group_line_bounds.set_y(height);
  }

  const std::vector<views::View*> children =
      collection_node_ ? collection_node_->GetDirectChildren()
                       : std::vector<views::View*>();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(size_bounds));

    auto drag_data = GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    // If the tab strip is not collapsed then the groups tabs should be inset.
    bounds.set_x(tab_strip_collapse_state !=
                         tabs::VerticalTabStripCollapseState::kExpanded
                     ? GetLayoutConstant(
                           LayoutConstant::kVerticalTabStripHorizontalPadding)
                     : VerticalTabGroupView::kTabLeadingPadding);
    // If width is bounded, child views should respect the width constraints
    // and take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x());
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width());
  }
  // Remove excess padding.
  height -= kTabVerticalPadding;

  if (!children.empty()) {
    group_line_bounds.set_height(height - group_line_bounds.y());
  }
  layouts.child_layouts.emplace_back(
      group_line_.get(), group_line_->GetVisible(), group_line_bounds);

  // Add extra padding below the group if not collapsed.
  const bool is_group_collapsed = IsCollapsed();
  if (!is_group_collapsed) {
    height += kTabVerticalPadding;
  }

  layouts.host_size = gfx::Size(
      width, is_group_collapsed
                 ? header_bounds.height() + (2 * kGroupHeaderVerticalMargin)
                 : height);
  return layouts;
}

void VerticalTabGroupView::ToggleCollapsedState(
    ToggleTabGroupCollapsedStateOrigin origin) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return;
  }

  collection_node_->GetController()->ToggleTabGroupCollapsedState(
      GetTabGroupFromNode(collection_node_), origin);
  InvalidateLayout();
}

views::Widget* VerticalTabGroupView::ShowGroupEditorBubble(
    bool stop_context_menu_propagation) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return nullptr;
  }

  // When the tab strip is collapsed, anchor to the group header, otherwise
  // anchor to the editor bubble button.
  views::View* anchor_view =
      GetTabStripCollapseState() !=
              tabs::VerticalTabStripCollapseState::kExpanded
          ? views::AsViewClass<views::View>(group_header_)
          : views::AsViewClass<views::View>(
                group_header_->editor_bubble_button());
  return collection_node_->GetController()->ShowGroupEditorBubble(
      GetTabGroupFromNode(collection_node_)->id(), anchor_view,
      stop_context_menu_propagation);
}

bool VerticalTabGroupView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsDragging();
}

bool VerticalTabGroupView::IsViewDragging(const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool VerticalTabGroupView::ShouldAnimateOpacityForAddAndRemove(
    const views::View& child_view) const {
  // Only animate opacity for tab views.
  return views::IsViewClass<VerticalTabView>(&child_view);
}

bool VerticalTabGroupView::ShouldSnapToTarget(
    const views::View& child_view) const {
  return views::IsViewClass<VerticalSplitTabView>(&child_view);
}

void VerticalTabGroupView::OnAnimationEnded() {
  // For collapsed tab groups update child visibility only once animations have
  // completed. This allows tabs to remain visible as the group animates closed.
  if (tab_group_visual_data_.is_collapsed()) {
    UpdateChildVisibilityForCollapseState(true);
  }
}

std::u16string VerticalTabGroupView::GetGroupContentString() const {
  if (!collection_node_) {
    return std::u16string();
  }

  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  if (group->tab_count() == 0) {
    return std::u16string();
  }

  return tab_groups::GetGroupContentString(group);
}

bool VerticalTabGroupView::IsValid() const {
  return collection_node_;
}

void VerticalTabGroupView::AttachChildView(
    std::unique_ptr<views::View> child_view,
    const gfx::Rect& previous_bounds_in_screen) {
  if (IsCollapsed()) {
    // When child views are added to a group when in collapsed state,
    // expand it to reveal the newly added views.
    ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  }
  layout_manager_->AnimateAndReparentView(std::move(child_view),
                                          previous_bounds_in_screen);
}

std::unique_ptr<views::View> VerticalTabGroupView::DetachChildView(
    views::View* child_view) {
  if (IsCollapsed()) {
    // The child views are invisible in collapsed state. When child views
    // are detached from the group while collapsed, reset its visibility.
    child_view->SetVisible(true);
  }
  return RemoveChildViewT(child_view);
}

void VerticalTabGroupView::ResetCollectionNode() {
  HideHoverCard(TabSlotController::HoverCardUpdateType::kTabRemoved);
  node_destroyed_subscription_ = {};
  tab_group_data_changed_subscription_ = {};
  tab_group_data_observer_.reset();
  collection_node_ = nullptr;
}

void VerticalTabGroupView::OnDataChanged() {
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

void VerticalTabGroupView::UpdateChildVisibilityForCollapseState(
    bool collapsed) {
  // Collection node may not exist at this point during browser shutdown.
  if (!collection_node_) {
    return;
  }
  group_line_->SetVisible(!collapsed);
  for (auto* child : collection_node_->GetDirectChildren()) {
    child->SetVisible(!collapsed);
  }
}

bool VerticalTabGroupView::IsCollapsed() const {
  return tab_group_visual_data_.is_collapsed();
}

views::ScrollView* VerticalTabGroupView::GetScrollViewForContainer() const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<views::View*>(parent()));
}

void VerticalTabGroupView::UpdateTargetLayoutForDrag(
    const std::vector<const views::View*>& views_to_snap) {
  layout_manager_->ResetViewsToTargetLayout(views_to_snap);
}

const views::ProposedLayout& VerticalTabGroupView::GetLayoutForDrag() const {
  return layout_manager_->target_layout();
}

const TabCollectionNode* VerticalTabGroupView::GetCollectionNodeFromView(
    const views::View& view) const {
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(&view)) {
    return tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(&view)) {
    return split_tab_view->collection_node();
  }
  return nullptr;
}

std::optional<BrowserRootView::DropIndex>
VerticalTabGroupView::GetLinkDropIndex(const gfx::Point& loc_in_group) {
  if (!collection_node_) {
    return std::nullopt;
  }
  // Use the vertical position to find the child view being dragged over.
  if (loc_in_group.y() < group_header_->bounds().bottom()) {
    // Determine whether the drop is on the leading (top) or trailing
    // (bottom) half of the header. If in the top half, then we the drag
    // is considered to be above the group.
    const bool is_leading =
        loc_in_group.y() < group_header_->bounds().CenterPoint().y();
    return GetDragHandler().GetLinkDropIndexForNode(
        *collection_node_, is_leading
                               ? std::make_optional(DragPositionHint::kBefore)
                               : std::nullopt);
  }

  for (const auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    if (loc_in_group.y() > view->bounds().bottom()) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_group);

    // If the drag is over the margins from the edges of the tab, then
    // consider this drag as a before/after rather than over.
    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;
    if (loc_in_child.y() < view->height() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (loc_in_child.y() > view->height() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide which
      // tab to replace.
      auto* split_view = views::AsViewClass<VerticalSplitTabView>(view);
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

void VerticalTabGroupView::InitHeaderDrag(const ui::LocatedEvent& event) {
  CHECK(collection_node_);
  const ui::ListSelectionModel original_selection_model =
      collection_node_->GetController()->GetSelectionModel();
  GetDragHandler().InitializeDrag(*collection_node_, original_selection_model,
                                  event);
}

bool VerticalTabGroupView::ContinueHeaderDrag(const ui::LocatedEvent& event) {
  return GetDragHandler().ContinueDrag(*group_header_, event);
}

void VerticalTabGroupView::CancelHeaderDrag() {
  GetDragHandler().EndDrag(EndDragReason::kCancel);
}

const TabGroup& VerticalTabGroupView::GetTabGroup() const {
  CHECK(collection_node_);
  return *GetTabGroupFromNode(collection_node_);
}

void VerticalTabGroupView::UpdateHoverCard(int update_type) const {
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

void VerticalTabGroupView::HideHoverCard(int update_type) const {
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

bool VerticalTabGroupView::IsFocusInTabStrip() {
  auto* tab_strip_view = GetVerticalTabStripView(this);
  return tab_strip_view && tab_strip_view->IsFocusInTabStrip();
}

std::unique_ptr<ExpandOnHoverLock>
VerticalTabGroupView::AcquireExpandOnHoverLock() {
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

void VerticalTabGroupView::ShiftGroupUp() {
  if (!collection_node_) {
    return;
  }
  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  collection_node_->GetController()->ShiftGroupUp(group->id());
}

void VerticalTabGroupView::ShiftGroupDown() {
  if (!collection_node_) {
    return;
  }
  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  collection_node_->GetController()->ShiftGroupDown(group->id());
}

BEGIN_METADATA(VerticalTabGroupView)
END_METADATA
