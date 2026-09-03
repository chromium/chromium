// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/dragged_tabs_container.h"

#include "base/callback_list.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

// Calculates the offset of the source dragged view (i.e. the main view being
// dragged) from the mouse.
gfx::Vector2d GetSourceViewOffsetFromMouse(
    views::View& source_dragged_view,
    const DragSessionData& session_data) {
  views::View* source_slot_view =
      session_data.source_view_drag_data()->attached_view;

  // The view that initiated the drag may not be the same as the view that
  // is being dragged (e.g. dragging a tab group header).
  gfx::Vector2d slot_view_offset_to_source =
      views::View::ConvertPointToTarget(source_slot_view, &source_dragged_view,
                                        source_slot_view->bounds().origin())
          .OffsetFromOrigin();
  gfx::Vector2d dragged_view_bounds_offset_from_bounds;
  dragged_view_bounds_offset_from_bounds -= slot_view_offset_to_source;
  dragged_view_bounds_offset_from_bounds -=
      {static_cast<int>(session_data.mouse_offset_to_size_ratios.x() *
                        source_slot_view->width()),
       static_cast<int>(session_data.mouse_offset_to_size_ratios.y() *
                        source_slot_view->height())};

  return dragged_view_bounds_offset_from_bounds;
}
}  // namespace

DraggedTabsContainer::DraggedTabsContainer(views::View& host_view,
                                           TabCollectionNode* collection_node,
                                           DragAxes drag_axes,
                                           DragLayout drag_layout)
    : host_view_(host_view),
      collection_node_(collection_node),
      drag_start_animation_(this),
      drag_axes_(drag_axes),
      drag_layout_(drag_layout) {
  host_view_observation_.Observe(&host_view);
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &DraggedTabsContainer::ResetCollectionNode, base::Unretained(this)));
}

DraggedTabsContainer::
    ~DraggedTabsContainer() {  // NOLINT(modernize-use-equals-default)
  on_will_destroy_callback_list_.Notify();
}

DraggedTabsContainer& DraggedTabsContainer::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  return *this;
}

TabDragContext* DraggedTabsContainer::OnTabDragUpdated(
    TabDragTarget::DragController& drag_controller,
    const gfx::Point& point_in_screen) {
  last_drag_point_in_screen_ = point_in_screen;
  if (drag_controller.GetAttachedContext() !=
      GetDragHandler().GetDragContext()) {
    // Do nothing until the drag attaches to this window's context.
    return GetDragHandler().GetDragContext();
  }

  // Used to determine whether the layout should snap into position without
  // animating at the end of this drag cycle.
  if (dragging_views_.empty()) {
    HandleTabDragEnteredContainer();
    InitializeDragState(drag_controller);
  }

  ApplyUpdatesForDragPositionChange();

  return GetDragHandler().GetDragContext();
}

void DraggedTabsContainer::ApplyUpdatesForDragPositionChange() {
  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_);

  gfx::Rect dragged_bounds_in_container =
      GetDraggingViewsBoundsAtPoint(point_in_container);

  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  scroll_handler_.OnDraggedTabPositionUpdated(
      *scroll_view, views::View::ConvertRectToTarget(
                        base::to_address(host_view_), scroll_view,
                        dragged_bounds_in_container));

  UpdateDraggingViewTransforms(point_in_container);

  HandleTabDragInContainer(dragged_bounds_in_container, point_in_container);
}

void DraggedTabsContainer::OnTabDragExited(const gfx::Point& point_in_screen) {
  ResetDragState();
  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  scroll_handler_.StopScrolling(*scroll_view);
}

void DraggedTabsContainer::OnTabDragEnded() {
  ResetDragState();
  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  scroll_handler_.StopScrolling(*scroll_view);
}

bool DraggedTabsContainer::CanDropTab() {
  return true;
}

void DraggedTabsContainer::HandleTabDragEnteredContainer() {
  last_move_drag_x_ = std::nullopt;
  CHECK(collection_node_);
  GetDragHandler().HandleDraggedTabsIntoNode(*collection_node_);

  // We don't need to snap any views here, since the layout manager already
  // skips animating dragged views.
  // The target layout just needs to be recalculated so that nodes added to this
  // container will haver their views tracked in the layout manager.
  UpdateTargetLayoutForDrag({});
}

base::CallbackListSubscription
DraggedTabsContainer::RegisterWillDestroyCallback(base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void DraggedTabsContainer::OnViewBoundsChanged(views::View* observed_view) {
  if (observed_view != base::to_address(host_view_)) {
    return;
  }
  // The transformation coordinates are relative to the host view's coordinates,
  // so they must be updated as the bounds change to ensure the dragged tabs
  // remain at the same point in the screen.
  if (IsHandlingDrag()) {
    UpdateDraggingViewTransforms(views::View::ConvertPointFromScreen(
        base::to_address(host_view_), last_drag_point_in_screen_));
  }
}

void DraggedTabsContainer::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view == base::to_address(host_view_)) {
    return;
  }

  if (dragging_views_.contains(observed_view)) {
    dragging_views_.erase(observed_view);
    dragged_view_observations_.RemoveObservation(observed_view);
  }
}

void DraggedTabsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK_EQ(animation, &drag_start_animation_);
  UpdateDraggingViewTransforms(views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_));
}

void DraggedTabsContainer::AnimationEnded(const gfx::Animation* animation) {
  CHECK_EQ(animation, &drag_start_animation_);
  host_view_->InvalidateLayout();
}

void DraggedTabsContainer::InitializeDragState(
    TabDragTarget::DragController& controller) {
  CHECK(dragging_views_.empty());
  last_move_drag_x_ = std::nullopt;

  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  on_scrolled_subscription_ =
      scroll_view->AddContentsScrolledCallback(base::BindRepeating(
          &DraggedTabsContainer::ApplyUpdatesForDragPositionChange,
          base::Unretained(this)));

  const auto& session_data = controller.GetSessionData();
  BuildDragLayout(session_data);

  InitializeDragStartAnimation(controller);
}

void DraggedTabsContainer::InitializeDragStartAnimation(
    const TabDragTarget::DragController& controller) {
  CHECK(animating_views_start_offsets_.empty());

  // Duration of the animation for dragged views to become contiguous.
  static constexpr base::TimeDelta kDragStartAnimationDuration =
      base::Milliseconds(200);
  base::TimeDelta drag_start_animation_duration =
      gfx::Animation::RichAnimationDuration(kDragStartAnimationDuration);
  static constexpr gfx::Tween::Type kStartDragAnimationTweenType =
      gfx::Tween::Type::EASE_IN_OUT;

  const auto& drag_handler = GetDragHandler();
  base::TimeDelta drag_time_elapsed =
      base::TimeTicks::Now() - controller.GetSessionData().drag_start_time;
  if (drag_time_elapsed >= drag_start_animation_duration) {
    return;
  }

  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_);
  gfx::Rect dragged_bounds_in_container =
      GetDraggingViewsBoundsAtPointClamped(point_in_container);

  const auto* source_dragged_view = drag_handler.ViewFromTabSlot(
      controller.GetSessionData().source_view_drag_data()->attached_view);

  const auto source_view_target_position = GetDraggingViewPositionForBounds(
      source_dragged_view, dragged_bounds_in_container,
      dragging_views_.at(source_dragged_view).offset);

  // Build the map of dragging views to their offset between their position at
  // the start of the drag, and the position they're expected to be at this
  // moment.
  const bool should_compute_x_offset = IsHorizontalDragSupported();
  const bool should_compute_y_offset = IsVerticalDragSupported();
  for (const auto& [dragging_view, visual_data] : dragging_views_) {
    auto start_offset_from_source =
        drag_handler.GetOffsetFromSourceAtDragStart(dragging_view);
    auto target_offset_from_source =
        GetDraggingViewPositionForBounds(
            dragging_view, dragged_bounds_in_container, visual_data.offset) -
        source_view_target_position;

    auto animation_offset =
        *start_offset_from_source - target_offset_from_source;
    if (!should_compute_x_offset) {
      animation_offset.set_x(0);
    }
    if (!should_compute_y_offset) {
      animation_offset.set_y(0);
    }
    if (animation_offset != gfx::Vector2d()) {
      animating_views_start_offsets_.insert({dragging_view, animation_offset});
    }
  }

  // Don't start the animation if there are no offsets to animate.
  if (animating_views_start_offsets_.empty()) {
    return;
  }

  drag_start_animation_.SetTweenType(kStartDragAnimationTweenType);
  drag_start_animation_.SetSlideDuration(drag_start_animation_duration);

  // Set the animations value to be proportional according to the time that
  // elapsed since the drag started. This makes the transition between
  // containers smoother.
  drag_start_animation_.Reset(gfx::Tween::CalculateValue(
      kStartDragAnimationTweenType,
      drag_time_elapsed.InMillisecondsF() /
          drag_start_animation_duration.InMilliseconds()));
  drag_start_animation_.Show();
}

void DraggedTabsContainer::BuildDragLayout(
    const DragSessionData& session_data) {
  auto* source_dragged_view = GetDragHandler().ViewFromTabSlot(
      session_data.source_view_drag_data()->attached_view);
  CHECK(source_dragged_view);

  dragging_views_bounds_ = gfx::Rect();
  dragging_views_bounds_.Offset(
      GetSourceViewOffsetFromMouse(*source_dragged_view, session_data));

  const auto& target_layout = GetLayoutForDrag();
  for (auto* attached_view : session_data.attached_views()) {
    auto* dragging_view = GetDragHandler().ViewFromTabSlot(attached_view);
    CHECK(dragging_view);

    if (dragging_view->parent() != base::to_address(host_view_)) {
      continue;
    }
    if (dragging_views_.contains(dragging_view)) {
      // It's possible that multiple dragged tabs map to the same dragged view
      // (e.g., split tabs). Skip the duplicates.
      continue;
    }

    const bool is_source_view = dragging_view == source_dragged_view;
    const auto* dragging_view_layout =
        target_layout.GetLayoutFor(dragging_view);
    CHECK(dragging_view_layout);

    switch (drag_layout_) {
      case DragLayout::kVertical:
        CHECK(!IsHorizontalDragSupported());
        AddViewToDragLayout(dragging_view, dragging_view_layout->bounds,
                            is_source_view);
        break;
      case DragLayout::kHorizontal:
        AddViewToHorizontalDragLayout(
            dragging_view, dragging_view_layout->bounds, is_source_view);
        break;
      case DragLayout::kSquash:
        AddViewToSquashedDragLayout(dragging_view, dragging_view_layout->bounds,
                                    is_source_view);
        break;
      default:
        NOTREACHED();
    }
  }
}

void DraggedTabsContainer::AddViewToDragLayout(views::View* dragging_view,
                                               const gfx::Rect& view_bounds,
                                               bool is_source_dragged_view) {
  gfx::Rect bounds = view_bounds;
  bounds.set_y(dragging_views_bounds_.height());
  dragging_views_.insert(
      {dragging_view, {.offset = bounds.OffsetFromOrigin()}});
  dragged_view_observations_.AddObservation(dragging_view);

  static constexpr int kDraggedViewVerticalPadding = 2;
  dragging_views_bounds_.set_height(dragging_views_bounds_.height() +
                                    bounds.height() +
                                    kDraggedViewVerticalPadding);

  if (is_source_dragged_view) {
    dragging_views_bounds_.Offset({-1 * bounds.x(), -1 * bounds.y()});
  }
}

void DraggedTabsContainer::AddViewToHorizontalDragLayout(
    views::View* dragging_view,
    const gfx::Rect& view_bounds,
    bool is_source_dragged_view) {
  gfx::Rect bounds = view_bounds;
  bounds.set_x(dragging_views_bounds_.width());
  dragging_views_.insert(
      {dragging_view, {.offset = bounds.OffsetFromOrigin()}});
  dragged_view_observations_.AddObservation(dragging_view);

  dragging_views_bounds_.set_width(dragging_views_bounds_.width() +
                                   bounds.width());
  dragging_views_bounds_.set_height(
      std::max(dragging_views_bounds_.height(), bounds.height()));

  if (is_source_dragged_view) {
    dragging_views_bounds_.Offset({-1 * bounds.x(), -1 * bounds.y()});
  }
}

void DraggedTabsContainer::AddViewToSquashedDragLayout(
    views::View* dragging_view,
    const gfx::Rect& view_bounds,
    bool is_source_dragged_view) {
  if (is_source_dragged_view) {
    dragging_views_bounds_.set_size(view_bounds.size());

    static constexpr int kDraggedViewHorizontalPadding = 4;
    dragging_views_bounds_.set_width(view_bounds.width() +
                                     kDraggedViewHorizontalPadding);
  }
  dragging_views_.insert(
      {dragging_view,
       {.offset = gfx::Vector2d(), .should_hide = !is_source_dragged_view}});
  dragged_view_observations_.AddObservation(dragging_view);
}

void DraggedTabsContainer::ResetDragState() {
  // Don't immediately clear `dragging_views_` so that the host view has a
  // chance to lay the dragged views out at their expected positions rather
  // than relying on `DraggedTabsContainer` to lay them out with
  // transforms.
  for (auto& [view, visual_data] : dragging_views_) {
    view->SetTransform(gfx::Transform());

    // The next layout update should allow the view to be shown by the host.
    visual_data.should_hide = false;
  }
  // The dragged view's bounds need to be snapped. While dragging, the bounds
  // are set to (0,0), but afterward the drag, the bounds must be updated to
  // the actual position, without animating.
  UpdateTargetLayoutForDrag(GetDraggingViews());
  dragging_views_.clear();
  dragged_view_observations_.RemoveAllObservations();
  animating_views_start_offsets_.clear();
  drag_start_animation_.Reset(0.0);
  dragging_views_bounds_ = gfx::Rect();
  last_move_drag_x_ = std::nullopt;

  on_scrolled_subscription_.reset();
}

void DraggedTabsContainer::UpdateDraggingViewTransforms(
    const gfx::Point& point_in_container) {
  const gfx::Rect bounding_box_for_point =
      GetDraggingViewsBoundsAtPointClamped(point_in_container);
  for (auto& [dragged_view, visual_data] : dragging_views_) {
    if (visual_data.should_hide && !drag_start_animation_.is_animating()) {
      continue;
    }
    // Use a transformation to render the dragged views, offset from the
    // container's origin.
    gfx::Transform transform;
    transform.Translate(GetDraggingViewPositionForBounds(
        dragged_view, bounding_box_for_point, visual_data.offset));

    dragged_view->SetTransform(transform);
  }
}

gfx::Rect DraggedTabsContainer::GetDraggingViewsBoundsAtPoint(
    const gfx::Point& point_in_container) const {
  gfx::Rect bounding_box_for_point = dragging_views_bounds_;
  bounding_box_for_point.Offset(point_in_container.OffsetFromOrigin());
  return bounding_box_for_point;
}

gfx::Rect DraggedTabsContainer::GetDraggingViewsBounds() const {
  gfx::Rect box_for_point = dragging_views_bounds_;
  box_for_point.Offset(
      views::View::ConvertPointFromScreen(base::to_address(host_view_),
                                          last_drag_point_in_screen_)
          .OffsetFromOrigin());
  return box_for_point;
}

gfx::Vector2d DraggedTabsContainer::GetDraggingViewPositionForBounds(
    const views::View* dragging_view,
    const gfx::Rect& dragging_views_bounding_box,
    const gfx::Vector2d& target_offset) const {
  gfx::Vector2d target(IsHorizontalDragSupported()
                           ? dragging_views_bounding_box.x() + target_offset.x()
                           : 0,
                       IsVerticalDragSupported()
                           ? dragging_views_bounding_box.y() + target_offset.y()
                           : 0);
  double value = drag_start_animation_.GetCurrentValue();
  if (drag_start_animation_.is_animating()) {
    if (auto it = animating_views_start_offsets_.find(dragging_view);
        it != animating_views_start_offsets_.end()) {
      target += {gfx::Tween::IntValueBetween(value, it->second.x(), 0),
                 gfx::Tween::IntValueBetween(value, it->second.y(), 0)};

      // If applying an offset for the drag-start animation, ensure we clamp
      // the offsets to the scroll view bounds.
      const auto* scroll_view = GetScrollViewForContainer();
      CHECK(scroll_view);
      gfx::Rect clamping_bounds = views::View::ConvertRectToTarget(
          scroll_view, base::to_address(host_view_),
          scroll_view->GetLocalBounds());
      gfx::Rect view_bounds(gfx::PointAtOffsetFromOrigin(target),
                            dragging_view->size());
      view_bounds.AdjustToFit(clamping_bounds);
      target = view_bounds.OffsetFromOrigin();
    }
  }

  return target;
}

gfx::Rect DraggedTabsContainer::GetDraggingViewsBoundsAtPointClamped(
    const gfx::Point& point_in_container) const {
  gfx::Rect bounding_box_for_point =
      GetDraggingViewsBoundsAtPoint(point_in_container);

  const auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  gfx::Rect clamping_bounds = views::View::ConvertRectToTarget(
      scroll_view, base::to_address(host_view_), scroll_view->GetLocalBounds());
  clamping_bounds.set_width(
      clamping_bounds.width() -
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding));
  bounding_box_for_point.AdjustToFit(clamping_bounds);

  return bounding_box_for_point;
}

std::optional<DraggedTabsContainer::DraggedViewVisualData>
DraggedTabsContainer::GetVisualDataForDraggedView(
    const views::View& view) const {
  auto it = dragging_views_.find(&view);
  if (it == dragging_views_.end()) {
    return std::nullopt;
  }

  // Views that should be hidden are still shown while animating into position,
  // but are set to "float" so that surrounding views may also animate into
  // the end position.
  const bool should_hide =
      it->second.should_hide && !drag_start_animation_.is_animating();
  const bool should_float =
      it->second.should_hide && drag_start_animation_.is_animating();
  if (view.GetTransform().IsIdentity()) {
    // If a drag recently ended the child will still be in
    // `dragging_views_` but will not have a transformation, which let's
    // the tab view animate into its correct slot.
    const gfx::Point point_in_container = views::View::ConvertPointFromScreen(
        base::to_address(host_view_), last_drag_point_in_screen_);
    const gfx::Rect bounding_box_for_point =
        GetDraggingViewsBoundsAtPointClamped(point_in_container);
    return std::make_optional(DraggedViewVisualData{
        .offset = GetDraggingViewPositionForBounds(
            &view, bounding_box_for_point, it->second.offset),
        .should_hide = should_hide,
        .should_float = should_float,
    });
  }
  // If the tab is being dragged, then it is rendered using
  // transformations, offset from the container's origin.
  return DraggedViewVisualData{
      .offset = gfx::Vector2d(),
      .should_hide = should_hide,
      .should_float = should_float,
  };
}

bool DraggedTabsContainer::IsHorizontalDragSupported() const {
  return drag_axes_ != DragAxes::kVerticalOnly;
}

bool DraggedTabsContainer::IsVerticalDragSupported() const {
  return drag_axes_ != DragAxes::kHorizontalOnly;
}

bool DraggedTabsContainer::HasMinimumOverlap(
    const gfx::Rect& a,
    const gfx::Rect& b,
    std::optional<int> min_x_overlap,
    std::optional<int> min_y_overlap) const {
  if (min_y_overlap) {
    gfx::RangeF vertical_overlap =
        gfx::RangeF(a.y(), a.bottom())
            .Intersect(gfx::RangeF(b.y(), b.bottom()));
    if (vertical_overlap.length() < *min_y_overlap) {
      return false;
    }
  }

  if (min_x_overlap) {
    gfx::RangeF horizontal_overlap =
        gfx::RangeF(a.x(), a.right()).Intersect(gfx::RangeF(b.x(), b.right()));
    if (horizontal_overlap.length() < *min_x_overlap) {
      return false;
    }
  }

  // Neither checks failed, so return `true`.
  return true;
}

bool DraggedTabsContainer::IsHandlingDrag() const {
  return !dragging_views_.empty();
}

TabDragHandler& DraggedTabsContainer::GetDragHandler() {
  return const_cast<TabDragHandler&>(std::as_const(*this).GetDragHandler());
}

const TabDragHandler& DraggedTabsContainer::GetDragHandler() const {
  CHECK(collection_node_);
  CHECK(collection_node_->GetController());
  return collection_node_->GetController()->GetDragHandler();
}

tabs::VerticalTabStripCollapseState
DraggedTabsContainer::GetTabStripCollapseState() const {
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  if (!controller) {
    return tabs::VerticalTabStripCollapseState::kExpanded;
  }
  return controller->GetStateController()->GetCollapseState();
}

void DraggedTabsContainer::ResetCollectionNode() {
  collection_node_ = nullptr;
}

std::vector<const views::View*> DraggedTabsContainer::GetDraggingViews() const {
  std::vector<const views::View*> views;
  views.reserve(dragging_views_.size());
  std::transform(dragging_views_.begin(), dragging_views_.end(),
                 std::back_inserter(views),
                 [](const auto& entry) { return entry.first; });

  return views;
}

void DraggedTabsContainer::HandleTabDragInContainer(
    const gfx::Rect& dragged_tab_bounds,
    const gfx::Point& point_in_container) {
  // Only reorder tabs if the drag has moved beyond a minimum threshold since
  // the last move to prevent rapid jitter and oscillation.
  if (drag_axes_ == DragAxes::kHorizontalOnly &&
      last_move_drag_x_.has_value()) {
    // Minimum distance a drag must travel between tab reorders in horizontal
    // mode to prevent jitter. This value gets scaled by the tab's size.
    constexpr int kHorizontalMoveThreshold = 16;
    const int tab_width = dragged_tab_bounds.width();
    const int standard_width =
        TabStyle::Get()->GetStandardWidth(/*is_split=*/false);
    const int threshold =
        base::ClampRound(static_cast<double>(tab_width) / standard_width *
                         kHorizontalMoveThreshold);
    if (std::abs(point_in_container.x() - *last_move_drag_x_) <= threshold) {
      return;
    }
  }

  TabDragHandler& drag_handler = GetDragHandler();
  const views::ProposedLayout& drag_layout = GetLayoutForDrag();
  const auto* target = GetTargetForTabDrag(drag_layout, dragged_tab_bounds);

  // A null target implies the drag is at the end of the container. We
  // intentionally invalidate layout here to support resizing the container as
  // the drag exceeds its bounds.
  if (drag_handler.HandleDraggedTabsIntoPosition(*collection_node_, target)) {
    if (drag_axes_ == DragAxes::kHorizontalOnly) {
      last_move_drag_x_ = point_in_container.x();
    }
    host_view_->InvalidateLayout();
  } else if (!target) {
    host_view_->InvalidateLayout();
  }
}

const TabCollectionNode* DraggedTabsContainer::GetTargetForTabDrag(
    const views::ProposedLayout& layout,
    const gfx::Rect& dragged_tab_bounds) const {
  // Whether the loop below saw the dragged view(s). Once the dragged view
  // was seen, the bounds of the later views must be adjusted to discount the
  // size of the dragged view.
  bool is_after_dragged_views = false;

  // Loop through the layout, until we find the first child view that appears
  // *below* the dragged view(s). If horizontal dragging is supported, then
  // first find the row that the drag lands on, then find the view within that
  // row.
  //
  // Assumes child layouts are ordered vertically, and represents their ordering
  // within the tab strip model.
  for (size_t i = 0; i < layout.child_layouts.size(); ++i) {
    const auto& child_layout = layout.child_layouts[i];
    const TabCollectionNode* child_node;
    if (GetDragHandler().IsViewDragging(*child_layout.child_view)) {
      is_after_dragged_views = true;
      continue;
    }
    if (!child_layout.visible ||
        !(child_node = GetCollectionNodeFromView(*child_layout.child_view))) {
      continue;
    }

    // If horizontal dragging is supported, then only look for the row that the
    // drag falls on. Once the row is found, delegate to
    // `GetTargetForTabDragInRow` to find the exact node within the row.
    if (IsHorizontalDragSupported()) {
      if (IsVerticalDragSupported() &&
          dragged_tab_bounds.y() > child_layout.bounds.CenterPoint().y()) {
        continue;
      }

      const int row_y = child_layout.bounds.y();
      return GetTargetForTabDragInRow(layout, dragged_tab_bounds, row_y, i,
                                      is_after_dragged_views);
    }

    // If the dragged view was already seen, then discount its size from
    // the candidate's position to represent where it would move to.
    const int dragged_height = dragged_tab_bounds.height();
    const int dragged_center_y = dragged_tab_bounds.CenterPoint().y();
    const int child_center_y = child_layout.bounds.CenterPoint().y();
    const int adjusted_child_center_y = is_after_dragged_views
                                            ? child_center_y - dragged_height
                                            : child_center_y;
    // Calculate the crossover midpoint between adjacent tabs.
    const int midpoint_y = adjusted_child_center_y + (dragged_height / 2);

    if (dragged_center_y < midpoint_y) {
      return child_node;
    }
  }
  return nullptr;
}

const TabCollectionNode* DraggedTabsContainer::GetTargetForTabDragInRow(
    const views::ProposedLayout& layout,
    const gfx::Rect& dragged_tab_bounds,
    int row_y,
    size_t row_start_idx,
    bool is_after_dragged_views) const {
  gfx::Rect logical_drag_bounds = dragged_tab_bounds;
  if (base::i18n::IsRTL()) {
    logical_drag_bounds.set_x(
        host_view_->GetMirroredXForRect(logical_drag_bounds));
  }

  const int tab_overlap = (drag_axes_ == DragAxes::kHorizontalOnly)
                              ? TabStyle::Get()->GetTabOverlap()
                              : 0;
  // The effective step size between adjacent tabs, taking tab overlap into
  // account.
  const int dragged_stride =
      std::max(0, logical_drag_bounds.width() - tab_overlap);
  const int dragged_center_x = logical_drag_bounds.CenterPoint().x();

  for (size_t i = row_start_idx; i < layout.child_layouts.size(); ++i) {
    const auto& row_child_layout = layout.child_layouts[i];
    const TabCollectionNode* row_child_node;
    if (GetDragHandler().IsViewDragging(*row_child_layout.child_view)) {
      is_after_dragged_views = true;
      continue;
    }
    if (!row_child_layout.visible ||
        !(row_child_node =
              GetCollectionNodeFromView(*row_child_layout.child_view))) {
      continue;
    }

    // If this loop reached a new row, assume the drag is at the end of the
    // previous row, so it should be placed before the first node of the new
    // row.
    if (row_child_layout.bounds.y() != row_y) {
      return row_child_node;
    }

    const int child_center_x = row_child_layout.bounds.CenterPoint().x();
    const int adjusted_child_center_x = is_after_dragged_views
                                            ? child_center_x - dragged_stride
                                            : child_center_x;
    // Calculate the crossover midpoint between adjacent tabs.
    const int midpoint_x = adjusted_child_center_x + (dragged_stride / 2);

    if (dragged_center_x < midpoint_x) {
      return row_child_node;
    }
  }
  return nullptr;
}
