// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include <numeric>

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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
  return static_cast<const tabs::TabGroupTabCollection*>(
             std::get<const tabs::TabCollection*>(node->GetNodeData()))
      ->GetTabGroup();
}

}  // namespace

VerticalTabGroupView::VerticalTabGroupView(TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this)),
      collection_node_(collection_node),
      tab_group_visual_data_(
          *GetTabGroupFromNode(collection_node_)->visual_data()),
      group_header_(AddChildView(std::make_unique<VerticalTabGroupHeaderView>(
          this,
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
  auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  bool is_tab_strip_collapsed = controller && controller->IsCollapsed();

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
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
    bounds.set_y(GetYForDraggedTabBounds(*child).value_or(height));
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
  // Remove excess padding if needed.
  height -= kTabVerticalPadding;

  if (!children.empty()) {
    group_line_bounds.set_height(height - group_line_bounds.y());
  }
  layouts.child_layouts.emplace_back(
      group_line_.get(), group_line_->GetVisible(), group_line_bounds);

  const bool is_collapsed = IsCollapsed();
  layouts.host_size = gfx::Size(
      width, is_collapsed
                 ? header_bounds.height() + (2 * kGroupHeaderVerticalMargin)
                 : height);
  return layouts;
}

void VerticalTabGroupView::ToggleCollapsedState(
    ToggleTabGroupCollapsedStateOrigin origin) {
  collection_node_->GetController()->ToggleTabGroupCollapsedState(
      GetTabGroupFromNode(collection_node_), origin);
}

views::Widget* VerticalTabGroupView::ShowGroupEditorBubble(
    bool stop_context_menu_propagation) {
  auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  bool is_tab_strip_collapsed = controller && controller->IsCollapsed();
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

void VerticalTabGroupView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabGroupView::OnDataChanged() {
  tab_group_visual_data_ =
      *GetTabGroupFromNode(collection_node_)->visual_data();
  group_header_->OnDataChanged(&tab_group_visual_data_);

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

VerticalTabDragHandler& VerticalTabGroupView::GetDragHandler() {
  return const_cast<VerticalTabDragHandler&>(
      std::as_const(*this).GetDragHandler());
}

const VerticalTabDragHandler& VerticalTabGroupView::GetDragHandler() const {
  CHECK(collection_node_);
  CHECK(collection_node_->GetController());
  return collection_node_->GetController()->GetDragHandler();
}

views::ScrollView* VerticalTabGroupView::GetScrollViewForContainer() const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<views::View*>(parent()));
}

void VerticalTabGroupView::UpdateLayoutForDrag() {
  layout_manager_->ResetToTargetLayout();
}

void VerticalTabGroupView::HandleTabDragInContainer(
    const gfx::Point point_in_container) {
  views::View* view_at_point =
      GetViewAtPoint(layout_manager_->target_layout(), point_in_container);
  const TabCollectionNode* node = collection_node_;
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    node = tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(view_at_point)) {
    node = split_tab_view->collection_node();
  }
  CHECK(node);
  GetDragHandler().HandleDraggedTabsOverNode(*node);
}

BEGIN_METADATA(VerticalTabGroupView)
END_METADATA
