// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_ANIMATING_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_ANIMATING_LAYOUT_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

// A simple LayoutManager for TabCollectionNode containers that animates changes
// to child view bounds. It wraps another LayoutManager (the
// target_layout_manager) which calculates the desired final positions. When the
// target layout changes, this manager animates the transition.
class TabCollectionAnimatingLayoutManager : public views::LayoutManagerBase,
                                            public gfx::AnimationDelegate {
 public:
  // Controls along which axis view bounds are animated during animate-in and
  // animate-out transitions.
  enum class AnimationAxis { kVertical, kHorizontal };

  // Represents how animations should progress along the animation axis.
  enum class AnimationDirection { kStartToEnd, kEndToStart };

  // Holds source layout information for views moved between TabCollectionNodes.
  // This is used for the initial layout and animation in the destination
  // collection, after which it is discarded and the view follows the layout
  // rules of its destination layout manager.
  struct SourceLayoutInfo {
    // Optional override for the axis along which the moved view should animate.
    // If unset host parameters will be used instead.
    std::optional<AnimationAxis> animation_axis;

    // The direction the moved view should animate.
    AnimationDirection animation_direction = AnimationDirection::kEndToStart;
  };

  class Delegate {
   public:
    virtual bool IsViewDragging(const views::View& child_view) const;
    virtual bool ShouldSnapToTarget(const views::View& child_view) const;
    virtual void OnAnimationEnded();

   protected:
    virtual ~Delegate() = default;
  };

  explicit TabCollectionAnimatingLayoutManager(
      std::unique_ptr<LayoutManagerBase> target_layout_manager,
      Delegate* delegate = nullptr,
      AnimationAxis animation_axis = AnimationAxis::kVertical,
      bool animate_host_size = false);
  TabCollectionAnimatingLayoutManager(
      const TabCollectionAnimatingLayoutManager&) = delete;
  TabCollectionAnimatingLayoutManager& operator=(
      const TabCollectionAnimatingLayoutManager&) = delete;
  ~TabCollectionAnimatingLayoutManager() override;

  // LayoutManagerBase:
  bool OnViewRemoved(views::View* host, views::View* view) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize(const views::View* host) const override;
  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Used by clients to directly set `SourceLayoutInfo` on `view_to_reparent`
  // before it is added to its destination container.
  static void SetSourceLayoutInfo(
      views::View* view_to_reparent,
      std::unique_ptr<SourceLayoutInfo> source_layout_info);

  // Recalculates the target layout, and `views_to_snap` to the new target,
  // skipping animations.
  void ResetViewsToTargetLayout(
      const std::vector<const views::View*>& views_to_snap);

  // Animates the removal of `child_view` from the `host_view()` associated with
  // this layout manager. `child_view` will be destroyed by the layout manager
  // asynchronously.
  void AnimateAndDestroyChildView(views::View* child_view);

  // Animates and reparents `view_to_reparent` from `previous_bounds_in_screen`
  // to target bounds in `host_view()`.
  void AnimateAndReparentView(std::unique_ptr<views::View> view_to_reparent,
                              const gfx::Rect& previous_bounds_in_screen);

  const views::ProposedLayout& target_layout() const { return target_layout_; }

 protected:
  // LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  void LayoutImpl() override;
  void OnInstalled(views::View* host) override;

 private:
  // Sets `starting_layout_` and `target_layout_` respectively. Clients must
  // set these via the below helpers to ensure `start_view_bounds_map_` and
  // `target_view_set_` reflect the current layout state.
  void SetStartingLayout(const views::ProposedLayout& starting_layout);
  void SetTargetLayout(const views::ProposedLayout& target_layout);

  // Recalculates the target layout and starts/updates animation if necessary.
  // Note: Layout change (e.g. child added/removed) requires recalculation.
  // However we don't need to call `RecalculateTarget()`  in `OnLayoutChanged()`
  // directly because `LayoutImpl()` will be called shortly after invalidation
  // happens.
  void RecalculateTarget();

  // Interpolates between `starting_layout_` and `target_layout_` based on
  // current `animation_` value.
  views::ProposedLayout InterpolateLayout(double value) const;

  // Removes and destroys any views marked for deletion that are no longer
  // needed for animated effects. This is called after a new layout has been
  // calculated.
  void RemoveNonAnimatingPendingDeleteViews();

  // Clears any child view metadata and state relevant only for the most
  // recent animation sequence, e.g. any state needed to animate Views moving
  // between independent TabCollectionNodes. Invoked after the current
  // `animation_` has ended.
  void ClearViewAnimationMetadata();

  // Clears any metadata specific to the animating layout manager from `view`.
  void ClearViewAnimationMetadataForView(views::View* view);

  // The layout manager that defines the goal state.
  const raw_ref<LayoutManagerBase> target_layout_manager_;

  // Animation handling.
  gfx::SlideAnimation animation_;

  // Layout states.
  views::ProposedLayout starting_layout_;  // State at start of animation.
  views::ProposedLayout target_layout_;    // Goal state.
  views::ProposedLayout current_layout_;   // Current interpolated state.

  // Precomputed maps for starting and target layouts for fast lookup. Updated
  // in `SetStartingLayout()` and `SetTargetLayout()`.
  using ChildViewLayoutMap =
      base::flat_map<raw_ptr<const views::View>, views::ChildLayout>;
  ChildViewLayoutMap start_view_layout_map_;
  ChildViewLayoutMap target_view_layout_map_;

  // Where in the animation the last layout recalculation happened.
  double starting_offset_ = 0.0;

  // The current animation progress.
  double current_offset_ = 1.0;

  const raw_ptr<Delegate> delegate_;

  // The axis along which bounds for animate-in and animate-out transitions are
  // interpolated.
  AnimationAxis animation_axis_;

  // Stores the height of the `current_layout_`. Recomputed on each call to
  // `InterpolateLayout()`. Mutable since this is a cached artifact of
  // calculating `current_layout_` and does not affect logical constness.
  mutable int current_layout_content_height_ = 0;

  // True if the manager should animate its preferred size, i.e. the manager
  // will update the host's preferred size to match the layout calculated in
  // `current_layout_`. This should be used only when the host's parent is not
  // using an animating layout manager and instead snaps the host immediately
  // to its target size. In such cases animating the host's preferred size
  // avoids clipping close / fade-out animations.
  const bool animate_host_size_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_ANIMATING_LAYOUT_MANAGER_H_
