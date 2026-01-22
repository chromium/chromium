// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

// Returns the expected Y coordinate for the view of a tab being dragged at
// `point`. Clamps minimum to `drag_clamp_min_y`.
int GetYForDraggedTab(const views::View& dragging_view,
                      const gfx::Point& point,
                      int drag_clamp_min_y) {
  return std::max(static_cast<float>(drag_clamp_min_y),
                  point.y() - (dragging_view.height() * 0.5f));
}

}  // namespace

VerticalDraggedTabsContainer::VerticalDraggedTabsContainer(
    views::View& host_view)
    : host_view_(host_view) {
  host_view_observation_.Observe(&host_view);
}

VerticalDraggedTabsContainer::~VerticalDraggedTabsContainer() {
  on_will_destroy_callback_list_.Notify();
}

VerticalDraggedTabsContainer& VerticalDraggedTabsContainer::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), point_in_screen);
  for (views::View* child : host_view_->children()) {
    if (!child->GetVisible() || !child->bounds().Contains(point_in_container) ||
        dragging_views_.contains(child)) {
      continue;
    }
    if (auto* group_view = views::AsViewClass<VerticalTabGroupView>(child)) {
      if (!group_view->IsCollapsed()) {
        return *group_view;
      }
    }
  }
  return *this;
}

TabDragContext* VerticalDraggedTabsContainer::OnTabDragUpdated(
    TabDragTarget::DragController& drag_controller,
    const gfx::Point& point_in_screen) {
  last_drag_point_in_screen_ = point_in_screen;
  if (drag_controller.GetAttachedContext() !=
      GetDragHandler().GetDragContext()) {
    // Do nothing until the drag attaches to this window's context.
    return GetDragHandler().GetDragContext();
  }

  // Hit-test against the target layout instead of the current one to prevent
  // bouncing between targets while mid-animation.
  // TODO(crbug.com/439963720): Finetune hit testing, using the bounds of the
  // dragged tabs.
  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), point_in_screen);

  HandleTabDragInContainer(point_in_container);

  // Used to determine whether the layout should snap into position without
  // animating at the end of this drag cycle.
  bool is_initial_drag = dragging_views_.empty();
  if (is_initial_drag) {
    InitializeDragState(drag_controller);
  }

  UpdateDraggingViewTransforms(point_in_container);

  if (is_initial_drag) {
    // This is needed so that the transformation takes over without animating
    // any bounds changes. This needs to be done after applying the initial
    // transformations because the transformation is taken into account
    // when determining the view's bounds.
    UpdateLayoutForDrag();
  }

  return GetDragHandler().GetDragContext();
}

void VerticalDraggedTabsContainer::OnTabDragExited() {
  ResetDragState();
}

void VerticalDraggedTabsContainer::OnTabDragEnded() {
  ResetDragState();
}

bool VerticalDraggedTabsContainer::CanDropTab() {
  return true;
}

base::CallbackListSubscription
VerticalDraggedTabsContainer::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void VerticalDraggedTabsContainer::OnViewBoundsChanged(
    views::View* observed_view) {
  CHECK_EQ(observed_view, base::to_address(host_view_));
  // The transformation coordinates are relative to the host view's coordinates,
  // so they must be updated as the bounds change to ensure the dragged tabs
  // remain at the same point in the screen.
  UpdateDraggingViewTransforms(views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_));
}

// TODO(crbug.com/476084253): Animate selected dragged tabs into the container.
void VerticalDraggedTabsContainer::InitializeDragState(
    TabDragTarget::DragController& controller) {
  // Move each dragged tab to the origin position. Transformations will be used
  // to render them during the drag.
  for (TabSlotView* slot_view : controller.GetSessionData().attached_views()) {
    auto* tab_view = GetDragHandler().ViewFromTabSlot(slot_view);
    CHECK(tab_view);
    if (tab_view->parent() == base::to_address(host_view_)) {
      dragging_views_.insert(tab_view);
    }
  }
}

void VerticalDraggedTabsContainer::ResetDragState() {
  for (auto view : dragging_views_) {
    view->SetTransform(gfx::Transform());
  }
  UpdateLayoutForDrag();
  dragging_views_.clear();
}

// TODO(crbug.com/476084253): Support laying out with multiple dragged tabs.
// Currently, all selected tabs are stacked on each other, but still block out
// the space at their expected tab slot.
void VerticalDraggedTabsContainer::UpdateDraggingViewTransforms(
    const gfx::Point& point_in_container) {
  int drag_clamp_min_y = GetMinYForDragToClamp();
  for (views::View* tab_view : dragging_views_) {
    // Use a transformation to render the dragged views, offset from the
    // container's origin.
    gfx::Transform transform;
    transform.Translate(
        0, GetYForDraggedTab(*tab_view, point_in_container, drag_clamp_min_y));
    tab_view->SetTransform(transform);
  }
}

int VerticalDraggedTabsContainer::GetMinYForDragToClamp() const {
  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  gfx::Point limit_point;
  limit_point = views::View::ConvertPointToTarget(
      scroll_view, base::to_address(host_view_), limit_point);
  return limit_point.y();
}

std::optional<int> VerticalDraggedTabsContainer::GetYForDraggedTabBounds(
    const views::View& view) const {
  if (!dragging_views_.contains(&view)) {
    return std::nullopt;
  }
  if (view.GetTransform().IsIdentity()) {
    // If a drag recently ended the child will still be in
    // `dragging_views_` but will not have a transformation, which let's
    // the tab view animate into its correct slot.
    return GetYForDraggedTab(
        view,
        views::View::ConvertPointFromScreen(base::to_address(host_view_),
                                            last_drag_point_in_screen_),
        GetMinYForDragToClamp());
  }
  // If the tab is being dragged, then it is rendered using
  // transformations, offset from the container's origin.
  return 0;
}

views::View* VerticalDraggedTabsContainer::GetViewAtPoint(
    const views::ProposedLayout& layout,
    const gfx::Point& point) {
  for (const auto& child_layout : layout.child_layouts) {
    if (child_layout.visible &&
        !dragging_views_.contains(child_layout.child_view) &&
        child_layout.bounds.y() < point.y() &&
        child_layout.bounds.bottom() > point.y()) {
      return child_layout.child_view;
    }
  }
  return nullptr;
}
