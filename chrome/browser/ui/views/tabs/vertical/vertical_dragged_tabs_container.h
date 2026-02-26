// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_drag_scroll_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view_observer.h"

class TabCollectionNode;
class VerticalTabDragHandler;

namespace views {
class View;
struct ProposedLayout;
class ScrollView;
}  // namespace views

// `VerticalDraggedTabsContainer` is an abstract class that can be derived to
// support handling dragged vertical tabs within the vertical tab strip view
// hierarchy. The class implements the `TabDragTarget` interface, allowing it to
// be targeted by the core tab dragging logic. It also supports nested targets,
// allowing the dragged tab view to be reparented into child views of this.
class VerticalDraggedTabsContainer : public TabDragTarget,
                                     public views::ViewObserver,
                                     public gfx::AnimationDelegate {
 public:
  // The axes that the dragged tabs can move on.
  enum class DragAxes { kVerticalOnly, kBoth };

  // How the dragged tabs should be laid out.
  enum class DragLayout { kVertical, kSquash };

  VerticalDraggedTabsContainer(views::View& host_view,
                               TabCollectionNode* collection_node,
                               DragAxes drag_axis,
                               DragLayout drag_layout);
  VerticalDraggedTabsContainer(const VerticalDraggedTabsContainer& other) =
      delete;
  VerticalDraggedTabsContainer& operator=(const VerticalDraggedTabsContainer&) =
      delete;
  ~VerticalDraggedTabsContainer() override;

  // Recursively searches through the view hierarchy to find the collection
  // that should should be handling the tab drag at the given point.
  virtual VerticalDraggedTabsContainer& GetTabDragTarget(
      const gfx::Point& point_in_screen);

  // TabDragTarget
  TabDragContext* OnTabDragUpdated(TabDragTarget::DragController& controller,
                                   const gfx::Point& point_in_screen) override;
  void OnTabDragEntered() override {}
  void OnTabDragExited(const gfx::Point& point_in_screen) override;
  void OnTabDragEnded() override;
  bool CanDropTab() final;
  void HandleTabDrop(TabDragTarget::DragController& controller) final {}
  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) final;

  // ViewObserver
  void OnViewBoundsChanged(views::View* observed_view) override;

  // AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Whether this container is currently handling a drag.
  bool IsHandlingDrag() const;

  // Returns the bounds of the box containing all dragged views, adjusted to
  // the point `point_in_container`. The returned bounds are not clamped to the
  // container bounds.
  gfx::Rect GetDraggingViewsBoundsAtPoint(
      const gfx::Point& point_in_container) const;
  // Returns the bounds of the box containing all dragged views, adjusted to the
  // last drag point. The returned bounds are not clamped to the container
  // bounds.
  gfx::Rect GetDraggingViewsBounds() const;

 protected:
  struct DraggedViewVisualData {
    gfx::Vector2d offset;
    bool should_hide = false;
    // If true, surrounding views do not have to be laid out around this
    // view.
    bool should_float = false;
  };

  // Returns the expected visual data, relative to the host view, for a dragged
  // view. Returns std::nullopt if the view is not being dragged.
  std::optional<DraggedViewVisualData> GetVisualDataForDraggedView(
      const views::View& view) const;

  // Helper for getting the target view for the given drag bounds, excluding
  // dragged views.
  views::View* GetViewForDragBounds(const views::ProposedLayout& layout,
                                    const gfx::Rect& dragged_tab_bounds);

  // Returns whether the two rects overlap by at least the provided minimums for
  // each axis.
  // E.g, if only `min_x_overlap` is provided, then this will return true
  // iff the range of [a.x(), a.right()] overlaps with the range [b.x(),
  // b.right()]. If both axes are specified, then this must be true for each
  // respective axis.
  bool HasMinimumOverlap(const gfx::Rect& a,
                         const gfx::Rect& b,
                         std::optional<int> min_x_overlap,
                         std::optional<int> min_y_overlap) const;

  VerticalTabDragHandler& GetDragHandler();
  const VerticalTabDragHandler& GetDragHandler() const;

 private:
  // Returns the scroll view for the container.
  virtual views::ScrollView* GetScrollViewForContainer() const = 0;

  // Updates the target layout of the host view, and snaps `views_to_snap` to
  // their target layout, skipping animations.
  virtual void UpdateTargetLayoutForDrag(
      const std::vector<const views::View*>& views_to_snap) = 0;

  // Get the layout of the host view, skipping animations.
  virtual const views::ProposedLayout& GetLayoutForDrag() const = 0;

  // Handles a dragged tab that is parented within this target.
  // `point_in_container` is a point relative to this target's view.
  virtual void HandleTabDragInContainer(
      const gfx::Rect& dragged_tab_bounds) = 0;

  // Handles dragged tabs entering this container, applying the necessary
  // updates to reparent them into this.
  void HandleTabDragEnteredContainer();

  // Updates state related to dragging tabs, to be used when this container
  // starts handling a drag.
  void InitializeDragState(TabDragTarget::DragController& controller);

  // Initializes the animation for dragged views to be ordered contiguously.
  void InitializeDragStartAnimation(
      const TabDragTarget::DragController& controller);

  // Builds `dragging_views_` and `dragging_views_bounds_` for the given
  // drag data.
  void BuildDragLayout(const DragSessionData& drag_data);
  void AddViewToVerticalDragLayout(views::View* dragging_view,
                                   const gfx::Rect& view_bounds,
                                   bool is_source_dragged_view);
  void AddViewToSquashedDragLayout(views::View* dragging_view,
                                   const gfx::Rect& view_bounds,
                                   bool is_source_dragged_view);

  // Whether the tab strip is collapsed.
  bool IsTabStripCollapsed() const;

  // Clears drag state and removes the transformations that were being used for
  // the drag.
  void ResetDragState();

  // Updates the transformations applied to dragging views, according to
  // the last drag point.
  void UpdateDraggingViewTransforms(const gfx::Point& point_in_container);

  bool IsHorizontalDragSupported() const;

  // Returns the bounds of the box containing all dragged views, adjusted to
  // the point `point_in_container` and clamped to the bounds of the
  // scroll view, which should be used for visual representation of the dragged
  // views.
  gfx::Rect GetDraggingViewsBoundsAtPointClamped(
      const gfx::Point& point_in_container) const;

  // Returns the expected position for the dragging view, given position of
  // the bounding box for views and a target offset within it.
  // This takes animation progress into account.
  gfx::Vector2d GetDraggingViewPositionForBounds(
      const views::View* dragging_view,
      const gfx::Rect& dragging_views_bounding_box,
      const gfx::Vector2d& target_offset) const;

  void ResetCollectionNode();

  // Handles updates, both visually and in the model, for whenever the dragged
  // tabs' position changes.
  void ApplyUpdatesForDragPositionChange();

  std::vector<const views::View*> GetDraggingViews() const;

  const raw_ref<views::View> host_view_;
  raw_ptr<TabCollectionNode> collection_node_;

  base::CallbackListSubscription node_destroyed_subscription_;
  int tab_strip_padding_;

  gfx::Point last_drag_point_in_screen_;

  // The bounding box of all the dragged views, relative to the drag point.
  gfx::Rect dragging_views_bounds_;

  // Child views that are being dragged, mapped to their DraggedViewVisualData,
  // whose offset is relative within `dragging_views_bounds_`.
  base::flat_map<raw_ptr<views::View>, DraggedViewVisualData> dragging_views_;

  // Dragged views must animate into contiguous/stacked order. This keeps track
  // of the starting position, used for animation.
  base::flat_map<raw_ptr<views::View>, gfx::Vector2d>
      animating_views_start_offsets_;
  gfx::SlideAnimation drag_start_animation_;

  const DragAxes drag_axes_;
  const DragLayout drag_layout_;

  base::ScopedObservation<views::View, views::ViewObserver>
      host_view_observation_{this};

  TabDragScrollHandler scroll_handler_;

  std::optional<base::CallbackListSubscription> on_scrolled_subscription_ =
      std::nullopt;

  base::OnceClosureList on_will_destroy_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_
