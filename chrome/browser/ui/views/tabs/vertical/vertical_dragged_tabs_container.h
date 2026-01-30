// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view_observer.h"

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
                                     public views::ViewObserver {
 public:
  // The axes that the dragged tabs can move on.
  enum class DragAxes { kVerticalOnly, kBoth };

  // How the dragged tabs should be laid out.
  enum class DragLayout { kVertical, kSquash };

  VerticalDraggedTabsContainer(views::View& host_view,
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
  void OnTabDragExited() override;
  void OnTabDragEnded() override;
  bool CanDropTab() final;
  void HandleTabDrop(TabDragTarget::DragController& controller) final {}
  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) final;

  // ViewObserver
  void OnViewBoundsChanged(views::View* observed_view) override;

 protected:
  // Returns the expected coordinates for a dragged tab view's bounds, or
  // null if the view isn't being dragged in this.
  // A value of 0 is used by default for the X-coordinate if dragging along the
  // x-axis is not supported.
  std::optional<gfx::Point> GetOriginForDraggedTabBounds(
      const views::View& view) const;

  // Helper for getting the view at a given point, excluding dragged views.
  views::View* GetViewAtPoint(const views::ProposedLayout& layout,
                              const gfx::Point& point);

 private:
  virtual VerticalTabDragHandler& GetDragHandler() = 0;
  virtual const VerticalTabDragHandler& GetDragHandler() const = 0;

  // Whether the tab strip is collapsed.
  virtual bool IsTabStripCollapsed() const = 0;

  // Returns the scroll view for the container.
  virtual views::ScrollView* GetScrollViewForContainer() const = 0;

  // Invalidates the layout of the host view, skipping animations.
  virtual void UpdateLayoutForDrag() = 0;

  // Handles a dragged tab that is parented within this target.
  // `point_in_container` is a point relative to this target's view.
  virtual void HandleTabDragInContainer(
      const gfx::Point point_in_container) = 0;

  // Updates state related to dragging tabs, to be used when this container
  // starts handling a drag.
  void InitializeDragState(TabDragTarget::DragController& controller);

  // Builds `dragging_views_` and `dragging_views_bounds_` for the given
  // drag data.
  void BuildDragLayout(const DragSessionData& drag_data);
  void AddViewToVerticalDragLayout(views::View* dragging_view,
                                   bool is_source_dragged_view);
  void AddViewToSquashedDragLayout(views::View* dragging_view,
                                   bool is_source_dragged_view);

  // Clears drag state and removes the transformations that were being used for
  // the drag.
  void ResetDragState();

  // Updates the transformations applied to dragging views, according to
  // the last drag point.
  void UpdateDraggingViewTransforms(const gfx::Point& point_in_container);

  // Returns the bounds of the box containing all dragged views, adjusted to
  // the point `point_in_container`, and clamped to the bounds of the
  // container.
  gfx::Rect GetDraggingViewsBoundsAtPoint(
      const gfx::Point& point_in_container) const;

  bool IsHorizontalDragSupported() const;

  const raw_ref<const views::View> host_view_;
  int tab_strip_padding_;

  gfx::Point last_drag_point_in_screen_;

  // The bounding box of all the dragged views, relative to the drag point.
  gfx::Rect dragging_views_bounds_;

  // Child views that are being dragged, mapped to their offset within
  // `dragging_views_bounds_`.
  base::flat_map<raw_ptr<views::View>, gfx::Vector2d> dragging_views_;

  const DragAxes drag_axes_;
  const DragLayout drag_layout_;

  base::ScopedObservation<views::View, views::ViewObserver>
      host_view_observation_{this};

  base::OnceClosureList on_will_destroy_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_DRAGGED_TABS_CONTAINER_H_
