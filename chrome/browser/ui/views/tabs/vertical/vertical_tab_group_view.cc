// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include <numeric>

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/widget/widget.h"

namespace {
constexpr int kTabVerticalPadding = 2;
constexpr int kGroupLineWidth = 2;
constexpr int kGroupLineCollapsedLeadingPadding = 2;
constexpr int kGroupLineCornerRadius = 4;
constexpr int kGroupHeaderHeight = 26;
constexpr int kGroupHeaderVerticalMargin = 4;
constexpr int kTabLeadingPadding = 10;

const TabGroup* GetTabGroupFromNode(TabCollectionNode* node) {
  CHECK(node);
  return static_cast<const tabs::TabGroupTabCollection*>(
             std::get<const tabs::TabCollection*>(node->GetNodeData()))
      ->GetTabGroup();
}

bool SupportsDataSharing() {
  return data_sharing::features::IsDataSharingFunctionalityEnabled();
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
              this))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabGroupView::ResetCollectionNode, base::Unretained(this)));

  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabGroupView::OnDataChanged, base::Unretained(this)));

  attention_indicator_observation_.Observe(GetTabGroupFromNode(collection_node_)
                                               ->GetTabGroupFeatures()
                                               ->attention_indicator());
  OnDataChanged();
}

VerticalTabGroupView::~VerticalTabGroupView() = default;

void VerticalTabGroupView::OnThemeChanged() {
  views::View::OnThemeChanged();
  OnDataChanged();
}

views::ProposedLayout VerticalTabGroupView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = kGroupHeaderVerticalMargin;
  bool is_tab_strip_collapsed = IsTabStripCollapsed();

  gfx::Rect header_bounds;
  gfx::Rect group_line_bounds;
  group_line_bounds.set_width(kGroupLineWidth);

  // If the tab strip is collapsed then the group line should appear on the
  // leading side of all grouped tabs and the header.
  if (is_tab_strip_collapsed) {
    group_line_bounds.set_x(kGroupLineCollapsedLeadingPadding);
    group_line_bounds.set_y(height);
    header_bounds.set_x(
        GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding));
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
  if (!is_tab_strip_collapsed) {
    group_line_bounds.set_x((kTabLeadingPadding - kGroupLineWidth) / 2);
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
    bounds.set_x(is_tab_strip_collapsed
                     ? GetLayoutConstant(
                           LayoutConstant::kVerticalTabStripCollapsedPadding)
                     : kTabLeadingPadding);
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
  const bool is_collapsed = IsCollapsed();
  if (!is_collapsed) {
    height += kTabVerticalPadding;
  }

  layouts.host_size = gfx::Size(
      width, is_collapsed
                 ? header_bounds.height() + (2 * kGroupHeaderVerticalMargin)
                 : height);
  return layouts;
}

void VerticalTabGroupView::OnAttentionStateChanged() {
  OnDataChanged();
}

void VerticalTabGroupView::ToggleCollapsedState(
    ToggleTabGroupCollapsedStateOrigin origin) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return;
  }

  collection_node_->GetController()->ToggleTabGroupCollapsedState(
      GetTabGroupFromNode(collection_node_), origin);
}

views::Widget* VerticalTabGroupView::ShowGroupEditorBubble(
    bool stop_context_menu_propagation) {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return nullptr;
  }

  bool is_tab_strip_collapsed = IsTabStripCollapsed();
  // When the tab strip is collapsed, anchor to the group header, otherwise
  // anchor to the editor bubble button.
  views::View* anchor_view =
      is_tab_strip_collapsed
          ? static_cast<views::View*>(group_header_)
          : static_cast<views::View*>(group_header_->editor_bubble_button());
  return collection_node_->GetController()->ShowGroupEditorBubble(
      GetTabGroupFromNode(collection_node_)->id(), anchor_view,
      stop_context_menu_propagation);
}

bool VerticalTabGroupView::IsViewDragging(const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
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

void VerticalTabGroupView::ResetCollectionNode() {
  attention_indicator_observation_.Reset();
  node_destroyed_subscription_ = {};
  data_changed_subscription_ = {};
  collection_node_ = nullptr;
}

void VerticalTabGroupView::OnDataChanged() {
  // If the group is in the process of being closed, then ignore updates.
  if (!collection_node_) {
    return;
  }

  const TabGroup* group = GetTabGroupFromNode(collection_node_);
  tab_group_visual_data_ = *group->visual_data();
  const bool has_attention =
      SupportsDataSharing() &&
      group->GetTabGroupFeatures()->attention_indicator()->GetHasAttention();
  group_header_->OnDataChanged(&tab_group_visual_data_, has_attention,
                               GetIsShared());

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

bool VerticalTabGroupView::IsTabStripCollapsed() const {
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  return controller && controller->IsCollapsed();
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

void VerticalTabGroupView::HandleTabDragInContainer(
    const gfx::Rect& dragged_tab_bounds) {
  CHECK(!IsCollapsed());
  views::View* view_at_point = GetViewForDragBounds(
      layout_manager_->target_layout(), dragged_tab_bounds);
  const TabCollectionNode* node = nullptr;
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    node = tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(view_at_point)) {
    node = split_tab_view->collection_node();
  }
  if (node) {
    GetDragHandler().HandleDraggedTabsOverNode(*node, std::nullopt);
    // Synchronously force a layout here to update the target layout. Since all
    // the calculations are based off on target layout, we need to ensure it is
    // updated where there are model change.
    DeprecatedLayoutImmediately();
  }
}

bool VerticalTabGroupView::GetIsShared() {
  CHECK(collection_node_);
  if (!SupportsDataSharing()) {
    return false;
  }

  tab_groups::TabGroupSyncService* tab_group_service =
      collection_node_->GetController()->GetTabGroupSyncService();
  if (!tab_group_service) {
    return false;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(GetTabGroupFromNode(collection_node_)->id());

  return saved_group && saved_group->is_shared_tab_group();
}

void VerticalTabGroupView::OnTabDragExited(const gfx::Point& point_in_screen) {
  if (!IsHandlingDrag()) {
    // If the drag entered then exited in subsequent drag loop iterations, then
    // the container will not have had a chance to handle the drag yet.
    return;
  }
  auto dragging_tabs_bounds = GetDraggingViewsBoundsAtPoint(
      views::View::ConvertPointFromScreen(this, point_in_screen));
  if (dragging_tabs_bounds.y() < 0) {
    GetDragHandler().HandleDraggedTabsOutOfGroup(*collection_node_,
                                                 DragPositionHint::kBefore);
  } else if (dragging_tabs_bounds.bottom() > height()) {
    GetDragHandler().HandleDraggedTabsOutOfGroup(*collection_node_,
                                                 DragPositionHint::kAfter);
  }
  VerticalDraggedTabsContainer::OnTabDragExited(point_in_screen);
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
      auto* split_view = static_cast<VerticalSplitTabView*>(view);
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

void VerticalTabGroupView::InitHeaderDrag(const ui::MouseEvent& event) {
  CHECK(collection_node_);
  GetDragHandler().InitializeDrag(*collection_node_, event);
}

bool VerticalTabGroupView::ContinueHeaderDrag(const ui::MouseEvent& event) {
  return GetDragHandler().ContinueDrag(*group_header_, event);
}

void VerticalTabGroupView::CancelHeaderDrag() {
  GetDragHandler().EndDrag(EndDragReason::kCancel);
}

void VerticalTabGroupView::HideHoverCard() const {
  if (!collection_node_) {
    return;
  }

  TabHoverCardController* hover_card_controller =
      collection_node_->GetController()->GetHoverCardController();

  if (hover_card_controller && hover_card_controller->IsHoverCardVisible()) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  }
}

BEGIN_METADATA(VerticalTabGroupView)
END_METADATA
