// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view_horizontal_layout.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view_vertical_layout.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

std::unique_ptr<PinnedTabContainerView::LayoutManager>
CreatePinnedTabContainerViewLayout(TabStripOrientation orientation) {
  if (orientation == TabStripOrientation::kVertical) {
    return std::make_unique<PinnedTabContainerViewVerticalLayout>();
  }
  return std::make_unique<PinnedTabContainerViewHorizontalLayout>();
}

}  // namespace

PinnedTabContainerView::PinnedTabContainerView(
    TabCollectionNode* collection_node)
    : DraggedTabsContainer(
          static_cast<views::View&>(*this),
          collection_node,
          collection_node && collection_node->orientation() ==
                                 TabStripOrientation::kHorizontal
              ? DragAxes::kHorizontalOnly
              : DragAxes::kBoth,
          collection_node && collection_node->orientation() ==
                                 TabStripOrientation::kHorizontal
              ? DragLayout::kHorizontal
              : DragLayout::kSquash),
      orientation_(collection_node->orientation()),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(std::make_unique<
                                        TabCollectionAnimatingLayoutManager>(
          CreatePinnedTabContainerViewLayout(orientation_),
          *this,
          collection_node->GetController()->animation_coordinator(),
          orientation_ == TabStripOrientation::kHorizontal
              ? TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal
              : TabCollectionAnimatingLayoutManager::AnimationAxis::
                    kHorizontalWrappingVertically,
          /*animate_host_size=*/false))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(base::to_address(layout_manager_))));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&PinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));

  if (orientation_ == TabStripOrientation::kVertical) {
    auto* state_controller =
        collection_node_->GetController()->GetStateController();
    CHECK(state_controller);
    OnCollapseStateChanged(state_controller->GetCollapseState());
    collapsed_state_changed_subscription_ =
        state_controller->RegisterOnCollapseChanged(
            base::BindRepeating(&PinnedTabContainerView::OnCollapseStateChanged,
                                base::Unretained(this)));
  }
}

PinnedTabContainerView::~PinnedTabContainerView() = default;

void PinnedTabContainerView::LayoutManager::OnInstalled(views::View* host) {
  CHECK(IsViewClass<class PinnedTabContainerView>(host));
}

const PinnedTabContainerView&
PinnedTabContainerView::LayoutManager::PinnedTabContainerView() const {
  return static_cast<const class PinnedTabContainerView&>(*host_view());
}

bool PinnedTabContainerView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsDragging();
}

bool PinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  return GetDragHandler().IsViewDragging(child_view);
}

bool PinnedTabContainerView::ShouldAnimateOpacityForAddAndRemove(
    const views::View& child_view) const {
  if (orientation_ == TabStripOrientation::kHorizontal) {
    return false;
  }
  // Only animate opacity for tab views.
  return views::IsViewClass<TabView>(&child_view);
}

bool PinnedTabContainerView::ShouldSnapToTarget(
    const views::View& child_view) const {
  return views::IsViewClass<SplitTabView>(&child_view);
}

bool PinnedTabContainerView::ShouldAnimateLayout() {
  if (!collection_node_) {
    return false;
  }
  return collection_node_->GetController()->ShouldAnimateLayout();
}

std::optional<BrowserRootView::DropIndex>
PinnedTabContainerView::GetLinkDropIndex(const gfx::Point& loc_in_container) {
  if (!collection_node_ || collection_node_->children().size() == 0) {
    return std::nullopt;
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    if (auto index = GetLinkDropIndexForHorizontal(loc_in_container)) {
      return index;
    }
  } else {
    auto collapse_state = GetTabStripCollapseState();
    if (collapse_state != tabs::VerticalTabStripCollapseState::kExpanded) {
      if (auto index = GetLinkDropIndexForCollapsed(loc_in_container)) {
        return index;
      }
    } else if (auto index = GetLinkDropIndexForExpanded(loc_in_container)) {
      return index;
    }
  }

  // Fallback to the end of the pinned container.
  return GetDragHandler().GetLinkDropIndexForNode(
      *collection_node_->children().back(), DragPositionHint::kAfter);
}

std::optional<BrowserRootView::DropIndex>
PinnedTabContainerView::GetLinkDropIndexForHorizontal(
    const gfx::Point& loc_in_container) {
  const bool is_vertical = orientation_ == TabStripOrientation::kVertical;
  const int padding = is_vertical ? kTabPadding : 0;
  const int logical_x = GetMirroredXInView(loc_in_container.x());

  for (auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);

    // In vertical orientation, check if the current point is within the
    // vertical bounds of the row this child belongs to.
    if (is_vertical &&
        loc_in_container.y() >= view->bounds().bottom() + padding / 2) {
      continue;
    }

    // If the point is to the right of the child, then let the next child
    // be the candidate.
    // For vertical multi-row grids, the full padding amount is used here,
    // rather than half as done above for row bottom bounds, so that this
    // correctly accounts for the last tab in the row.
    // The x-based calculation uses a margin to determine if the link should
    // be inserted before/after, so the cutoff point between tabs in a row
    // doesn't have to be exact.
    if (logical_x >= view->bounds().right() + padding) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);

    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;

    const int logical_x_in_child = view->GetMirroredXInView(loc_in_child.x());

    // Determine whether the drag is to the right, left, or over the tab/tab
    // split.
    if (logical_x_in_child < view->width() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (logical_x_in_child > view->width() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide
      // which tab to replace.
      auto* split_view = views::AsViewClass<SplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_container);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  return std::nullopt;
}

std::optional<BrowserRootView::DropIndex>
PinnedTabContainerView::GetLinkDropIndexForCollapsed(
    const gfx::Point& loc_in_container) {
  // While collapsed, simply use the y-coordinate, similar to the unpinned
  // container.
  for (auto& child_node : collection_node_->children()) {
    auto* view = child_node->view();
    CHECK(view);
    if (loc_in_container.y() >= view->bounds().bottom()) {
      continue;
    }

    gfx::Point loc_in_child =
        views::View::ConvertPointToTarget(this, view, loc_in_container);
    constexpr double kDragOverMargins = 0.2;
    std::optional<DragPositionHint> hint;

    // Determine whether the drag is above, below, or above the tab/tab split.
    if (loc_in_child.y() < view->height() * kDragOverMargins) {
      hint = DragPositionHint::kBefore;
    } else if (loc_in_child.y() > view->height() * (1 - kDragOverMargins)) {
      hint = DragPositionHint::kAfter;
    } else if (child_node->type() == TabCollectionNode::Type::SPLIT) {
      // If landing in the middle of the split, let the split view decide
      // which tab to replace.
      auto* split_view = views::AsViewClass<SplitTabView>(view);
      gfx::Point loc_in_split =
          views::View::ConvertPointToTarget(this, split_view, loc_in_container);
      return split_view->GetLinkDropIndex(loc_in_split);
    } else {
      hint = std::nullopt;
    }
    return GetDragHandler().GetLinkDropIndexForNode(*child_node, hint);
  }

  return std::nullopt;
}

std::optional<BrowserRootView::DropIndex>
PinnedTabContainerView::GetLinkDropIndexForExpanded(
    const gfx::Point& loc_in_container) {
  return GetLinkDropIndexForHorizontal(loc_in_container);
}

void PinnedTabContainerView::ResetCollectionNode() {
  collapsed_state_changed_subscription_ = {};
  collection_node_ = nullptr;
}

void PinnedTabContainerView::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  // Collapsed state only exists for the vertical orientation.
  CHECK_EQ(orientation_, TabStripOrientation::kVertical);
  set_drag_axes(state == tabs::VerticalTabStripCollapseState::kCollapsed
                    ? DragAxes::kVerticalOnly
                    : DragAxes::kBoth);
  InvalidateLayout();
}

views::ScrollView* PinnedTabContainerView::GetScrollViewForContainer() const {
  if (!collection_node_) {
    return nullptr;
  }
  return views::ScrollView::GetScrollViewForContents(
      const_cast<PinnedTabContainerView*>(this));
}

void PinnedTabContainerView::UpdateTargetLayoutForDrag(
    const std::vector<const views::View*>& views_to_snap) {
  layout_manager_->ResetViewsToTargetLayout(views_to_snap);
}

const views::ProposedLayout& PinnedTabContainerView::GetLayoutForDrag() const {
  return layout_manager_->target_layout();
}

const TabCollectionNode* PinnedTabContainerView::GetCollectionNodeFromView(
    const views::View& view) const {
  if (auto* tab_view = views::AsViewClass<TabView>(&view)) {
    return tab_view->collection_node();
  } else if (auto* split_tab_view = views::AsViewClass<SplitTabView>(&view)) {
    return split_tab_view->collection_node();
  }
  return nullptr;
}

BEGIN_METADATA(PinnedTabContainerView)
END_METADATA
