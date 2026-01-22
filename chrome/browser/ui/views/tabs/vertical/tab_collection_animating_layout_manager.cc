// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"

#include <algorithm>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "ui/base/class_property.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

// Views of removed TabCollectionNodes may temporarily remain in the View tree
// to allow them to animate-out. This property is used to ensure these views
// are removed from the View tree once they are no longer required.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPendingDeletion, false)

// Stores the bounds in screen coordinates of the associated View prior to being
// removed from its host TabCollectionNode. Used for collection move animations.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kPreviousCollectionBounds)

}  // namespace

bool TabCollectionAnimatingLayoutManager::Delegate::IsViewDragging(
    const views::View& child_view) const {
  return false;
}

bool TabCollectionAnimatingLayoutManager::Delegate::ShouldSnapToTarget(
    const views::View& child_view) const {
  return false;
}

void TabCollectionAnimatingLayoutManager::Delegate::OnAnimationEnded() {}

TabCollectionAnimatingLayoutManager::TabCollectionAnimatingLayoutManager(
    std::unique_ptr<LayoutManagerBase> target_layout_manager,
    Delegate* delegate,
    AnimationAxis animation_axis)
    : target_layout_manager_(
          CHECK_DEREF(AddOwnedLayout(std::move(target_layout_manager)))),
      animation_(this),
      delegate_(delegate),
      animation_axis_(animation_axis) {
  // TODO(crbug.com/459824840): Determine the appropriate animation duration.
  // Currently set to match the duration of TabContainerImpl.
  animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(base::Milliseconds(200)));
  animation_.SetTweenType(gfx::Tween::EASE_IN_OUT);
}

TabCollectionAnimatingLayoutManager::~TabCollectionAnimatingLayoutManager() =
    default;

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  return target_layout_manager_->GetPreferredSize(host);
}

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  return target_layout_manager_->GetPreferredSize(host, available_size);
}

gfx::Size TabCollectionAnimatingLayoutManager::GetMinimumSize(
    const views::View* host) const {
  return target_layout_manager_->GetMinimumSize(host);
}

int TabCollectionAnimatingLayoutManager::GetPreferredHeightForWidth(
    const views::View* host,
    int width) const {
  return target_layout_manager_->GetPreferredHeightForWidth(host, width);
}

void TabCollectionAnimatingLayoutManager::AnimationProgressed(
    const gfx::Animation* animation) {
  if (current_offset_ == animation->GetCurrentValue()) {
    return;
  }
  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);
}

void TabCollectionAnimatingLayoutManager::AnimationEnded(
    const gfx::Animation* animation) {
  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);

  // Clear any View-specific metadata and state no longer needed once the most
  // recent animation has finished.
  ClearViewAnimationMetadata();
  if (delegate_) {
    delegate_->OnAnimationEnded();
  }
}

views::ProposedLayout
TabCollectionAnimatingLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // If we are animating, return the current interpolated state. Otherwise,
  // return the target state.
  return animation_.is_animating() ? current_layout_
                                   : target_layout_manager_->GetProposedLayout(
                                         size_bounds, PassKey());
}

void TabCollectionAnimatingLayoutManager::LayoutImpl() {
  RecalculateTarget();

  if (animation_.is_animating()) {
    current_offset_ = animation_.GetCurrentValue();
    double denominator = 1.0 - starting_offset_;
    double percent = (current_offset_ - starting_offset_) / denominator;
    percent = std::clamp(percent, 0.0, 1.0);
    current_layout_ = InterpolateLayout(percent);
    ApplyLayout(current_layout_);
  } else {
    // Ensure we are snapped to target.
    current_offset_ = 1.0;
    starting_offset_ = 0.0;
    current_layout_ = target_layout_;
    starting_layout_ = target_layout_;
    ApplyLayout(target_layout_);
    RemoveNonAnimatingPendingDeleteViews();
    ClearViewAnimationMetadata();
  }
}

void TabCollectionAnimatingLayoutManager::OnInstalled(views::View* host) {
  LayoutManagerBase::OnInstalled(host);
  RecalculateTarget();
}

void TabCollectionAnimatingLayoutManager::RecalculateTarget() {
  if (!host_view()) {
    return;
  }

  // Calculate the target layout with unbounded height and the width given to
  // the host by its parent view.
  views::ProposedLayout new_target = target_layout_manager_->GetProposedLayout(
      views::SizeBounds(host_view()->width(), {}), PassKey());

  // If the layout hasn't changed, we are done.
  if (new_target == target_layout_) {
    return;
  }

  // Animating horizontal bounds is not supported and layout should immediately
  // snap to target for horizontal bounds changes.
  if (!current_layout_.host_size.IsEmpty() &&
      (current_layout_.host_size.width() != new_target.host_size.width())) {
    target_layout_ = new_target;
    current_layout_ = new_target;
    starting_layout_ = new_target;
    starting_offset_ = 0.0;
    current_offset_ = 1.0;
    return;
  }

  target_layout_ = new_target;

  // If we haven't actually rendered a frame yet keep the original
  // starting_layout.
  if (animation_.is_animating() && current_offset_ == starting_offset_) {
    return;
  }

  constexpr double kResetAnimationThreshold = 0.8;

  if (current_offset_ > kResetAnimationThreshold) {
    // We are far enough along that we should start a "fresh" animation
    // from 0% to avoid awkward slow-downs at the end of the curve.
    starting_layout_ = current_layout_;
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_.Reset(0.0);
  } else {
    // We are still early in the animation. Simply update the starting offset.
    // The timer remains running and we just calculate a new slope in
    // LayoutImpl.
    starting_layout_ = current_layout_;
    starting_offset_ = current_offset_;
  }

  if (!animation_.is_animating()) {
    animation_.Show();
  }
}

void TabCollectionAnimatingLayoutManager::ResetToTargetLayout() {
  RecalculateTarget();
  animation_.Reset(0.0);
  starting_layout_ = target_layout_;
  current_layout_ = target_layout_;
  InvalidateHost(/*mark_layouts_changed=*/false);
}

void TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView(
    views::View* child_view) {
  DCHECK(std::ranges::contains(host_view()->children(), child_view));
  child_view->SetCanProcessEventsWithinSubtree(false);
  child_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  child_view->SetProperty(kPendingDeletion, true);
  InvalidateHost(/*mark_layouts_changed=*/true);
}

void TabCollectionAnimatingLayoutManager::AnimateAndReparentView(
    std::unique_ptr<views::View> view_to_reparent,
    const gfx::Rect& previous_bounds_in_screen) {
  auto* child_view = host_view()->AddChildView(std::move(view_to_reparent));
  if (!delegate_ || !delegate_->IsViewDragging(*child_view)) {
    child_view->SetPaintToLayer();
    child_view->SetProperty(kPreviousCollectionBounds,
                            previous_bounds_in_screen);
  }
}

views::ProposedLayout TabCollectionAnimatingLayoutManager::InterpolateLayout(
    double value) const {
  views::ProposedLayout result;

  // Assume the host size snaps to its target size for the duration of the
  // animation.
  result.host_size = target_layout_.host_size;

  // Map view pointers to their starting bounds for fast lookup
  std::vector<std::pair<const views::View*, gfx::Rect>> start_bounds_pairs;
  start_bounds_pairs.reserve(starting_layout_.child_layouts.size());
  for (const views::ChildLayout& layout : starting_layout_.child_layouts) {
    start_bounds_pairs.emplace_back(layout.child_view.get(), layout.bounds);
  }
  base::flat_map<const views::View*, gfx::Rect> start_bounds_map(
      std::move(start_bounds_pairs));

  for (const auto& target_child : target_layout_.child_layouts) {
    views::ChildLayout interpolated_child = target_child;
    auto it = start_bounds_map.find(target_child.child_view);

    if (it != start_bounds_map.end()) {
      // Moved child.
      // Interpolate between start and target bounds.
      interpolated_child.bounds =
          gfx::Tween::RectValueBetween(value, it->second, target_child.bounds);
      // Snap visibility to target.
      interpolated_child.visible = target_child.visible;
    } else if (!delegate_ ||
               (!delegate_->IsViewDragging(*target_child.child_view) &&
                !delegate_->ShouldSnapToTarget(*target_child.child_view))) {
      // Added child.
      // Animate-in new Views from empty bounds.
      gfx::Rect* previous_container_bounds =
          target_child.child_view->GetProperty(kPreviousCollectionBounds);
      if (previous_container_bounds) {
        gfx::Rect initial_bounds = views::View::ConvertRectFromScreen(
            host_view(), *previous_container_bounds);
        interpolated_child.bounds = gfx::Tween::RectValueBetween(
            value, initial_bounds, target_child.bounds);
      } else {
        gfx::Rect initial_bounds = target_child.bounds;
        if (animation_axis_ == AnimationAxis::kVertical) {
          initial_bounds.set_height(0);
        } else {
          initial_bounds.set_width(0);
        }
        interpolated_child.bounds = gfx::Tween::RectValueBetween(
            value, initial_bounds, target_child.bounds);
      }
    } else {
      // This branch results in new children being snapped to target bounds
      // (e.g. drag-and-drop or split-tabs which explicitly requires no animated
      // transition).
    }
    result.child_layouts.push_back(interpolated_child);
  }

  // Create a set of current child views for fast lookup.
  std::vector<const views::View*> child_views;
  child_views.reserve(host_view()->children().size());
  std::ranges::transform(
      host_view()->children(), std::back_inserter(child_views),
      [](const auto& child_view) { return child_view.get(); });
  base::flat_set<const views::View*> child_view_set(std::move(child_views));

  for (const auto& start_child : starting_layout_.child_layouts) {
    // Animate-out only pending delete views that were present in the previous
    // layout.
    if (child_view_set.contains(start_child.child_view) &&
        start_child.child_view->GetProperty(kPendingDeletion)) {
      // Removed child.
      // Pending delete Views will remain in the Views hierarchy until they are
      // no longer needed for animation (i.e. they are no longer in
      // `starting_layout_`), at which point they will be removed by
      // `RemoveNonAnimatingPendingDeleteViews()`.
      views::ChildLayout interpolated_child = start_child;
      gfx::Rect target_bounds = start_child.bounds;
      if (animation_axis_ == AnimationAxis::kVertical) {
        target_bounds.set_height(0);
      } else {
        target_bounds.set_width(0);
      }
      interpolated_child.bounds = gfx::Tween::RectValueBetween(
          value, start_child.bounds, target_bounds);
      result.child_layouts.push_back(interpolated_child);
    }
  }

  return result;
}

void TabCollectionAnimatingLayoutManager::
    RemoveNonAnimatingPendingDeleteViews() {
  // Collect all children marked as pending delete.
  std::vector<std::pair<views::View*, bool>> pending_delete_child_view_pairs;
  pending_delete_child_view_pairs.reserve(host_view()->children().size());
  for (views::View* child : host_view()->children()) {
    if (child->GetProperty(kPendingDeletion)) {
      pending_delete_child_view_pairs.emplace_back(child, true);
    }
  }

  // Early return if there are no pending-delete children to remove.
  if (pending_delete_child_view_pairs.empty()) {
    return;
  }

  // Create a map of pending delete child views, with the value indicating
  // whether the view is absent from `starting_layout_` and should be removed.
  base::flat_map<views::View*, bool> pending_delete_child_view_map(
      std::move(pending_delete_child_view_pairs));

  // Ensure that any pending delete views still in `starting_layout_` are
  // retained.
  for (const views::ChildLayout& layout : starting_layout_.child_layouts) {
    auto it = pending_delete_child_view_map.find(layout.child_view);
    if (it != pending_delete_child_view_map.end()) {
      it->second = false;
    }
  }

  // Remove any pending delete views no longer in `starting_layout_`.
  for (auto& [child, should_remove] : pending_delete_child_view_map) {
    if (should_remove) {
      host_view()->RemoveChildViewT(child);
    }
  }
}

void TabCollectionAnimatingLayoutManager::ClearViewAnimationMetadata() {
  for (views::View* child_view : host_view()->children()) {
    if (child_view->GetProperty(kPreviousCollectionBounds)) {
      child_view->DestroyLayer();
      child_view->ClearProperty(kPreviousCollectionBounds);
    }
  }
}
