// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_views.h"

#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/view_utils.h"

TabGroupViews::TabGroupViews(views::View* container_view,
                             views::View* drag_container_view,
                             TabSlotController& tab_slot_controller,
                             const tab_groups::TabGroupId& group)
    : tab_slot_controller_(tab_slot_controller), group_(group) {
  style_ = std::make_unique<const TabGroupStyle>(*this);
  const TabGroupStyle* style = style_.get();

  header_ = container_view->AddChildView(
      std::make_unique<TabGroupHeader>(*tab_slot_controller_, group_, *style));
  underline_ = container_view->AddChildView(
      std::make_unique<TabGroupUnderline>(this, group_, *style));
  drag_underline_ = drag_container_view->AddChildView(
      std::make_unique<TabGroupUnderline>(this, group_, *style));
  highlight_ = drag_container_view->AddChildView(
      std::make_unique<TabGroupHighlight>(this, group_, *style));
  highlight_->SetVisible(false);
}

TabGroupViews::~TabGroupViews() {
  header_->parent()->RemoveChildViewT(std::exchange(header_, nullptr));
  underline_->parent()->RemoveChildViewT(std::exchange(underline_, nullptr));
  drag_underline_->parent()->RemoveChildViewT(
      std::exchange(drag_underline_, nullptr));
  highlight_->parent()->RemoveChildViewT(std::exchange(highlight_, nullptr));
}

void TabGroupViews::UpdateBounds() {
  // If we're tearing down we should ignore events.
  if (InTearDown())
    return;

  auto [leading_group_view, trailing_group_view] =
      GetLeadingTrailingGroupViews();
  underline_->UpdateBounds(leading_group_view, trailing_group_view);

  auto [leading_dragged_group_view, trailing_dragged_group_view] =
      GetLeadingTrailingDraggedGroupViews();
  drag_underline_->UpdateBounds(leading_dragged_group_view,
                                trailing_dragged_group_view);

  if (underline_->GetVisible() && drag_underline_->GetVisible()) {
    // If we are painting `drag_underline_` on top of `underline_`, we may need
    // to extend `drag_underline_`'s bounds so the two merge seamlessly.
    // Otherwise, `underline_` may be partially occluded by the dragged views.
    gfx::RectF underline_bounds_in_drag_coords_f(underline_->GetLocalBounds());
    views::View::ConvertRectToTarget(underline_, drag_underline_->parent(),
                                     &underline_bounds_in_drag_coords_f);
    gfx::Rect underline_bounds_in_drag_coords =
        ToEnclosingRect(underline_bounds_in_drag_coords_f);

    // Try to match `underline_`, but don't shrink, and don't expand beyond the
    // dragged views unless already beyond them.
    int leading_x =
        std::clamp(underline_bounds_in_drag_coords.x(),
                   std::min(drag_underline_->bounds().x(),
                            leading_dragged_group_view->bounds().x()),
                   drag_underline_->bounds().x());
    int trailing_x =
        std::clamp(underline_bounds_in_drag_coords.right(),
                   drag_underline_->bounds().right(),
                   std::max(drag_underline_->bounds().right(),
                            trailing_dragged_group_view->bounds().right()));

    drag_underline_->SetBounds(leading_x, drag_underline_->bounds().y(),
                               trailing_x - leading_x,
                               drag_underline_->bounds().height());
  }

  highlight_->UpdateBounds(leading_dragged_group_view,
                           trailing_dragged_group_view);
}

void TabGroupViews::OnGroupVisualsChanged() {
  // If we're tearing down we should ignore events. (We can still receive them
  // here if the editor bubble was open when the tab group was closed.)
  if (InTearDown())
    return;

  header_->VisualsChanged();
  underline_->SchedulePaint();
  drag_underline_->SchedulePaint();
}

gfx::Rect TabGroupViews::GetBounds() const {
  auto [leading_group_view, trailing_group_view] =
      GetLeadingTrailingGroupViews();

  gfx::RectF leading_bounds = gfx::RectF(leading_group_view->GetLocalBounds());
  views::View::ConvertRectToTarget(leading_group_view, underline_->parent(),
                                   &leading_bounds);

  gfx::RectF trailing_bounds =
      gfx::RectF(trailing_group_view->GetLocalBounds());
  views::View::ConvertRectToTarget(trailing_group_view, underline_->parent(),
                                   &trailing_bounds);

  gfx::Rect bounds = gfx::ToEnclosingRect(leading_bounds);
  bounds.UnionEvenIfEmpty(gfx::ToEnclosingRect(trailing_bounds));

  return bounds;
}

SkColor TabGroupViews::GetGroupColor() const {
  return tab_slot_controller_->GetPaintedGroupColor(
      tab_slot_controller_->GetGroupColorId(group_));
}

bool TabGroupViews::InTearDown() const {
  return !header_ || !header_->GetWidget() || !drag_underline_->GetWidget();
}

std::tuple<const views::View*, const views::View*>
TabGroupViews::GetLeadingTrailingGroupViews() const {
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      underline_->parent()->children();
  std::vector<raw_ptr<views::View, VectorExperimental>> dragged_children =
      drag_underline_->parent()->children();
  children.insert(children.end(), dragged_children.begin(),
                  dragged_children.end());
  return GetLeadingTrailingGroupViews(children);
}

std::tuple<views::View*, views::View*>
TabGroupViews::GetLeadingTrailingDraggedGroupViews() const {
  return GetLeadingTrailingGroupViews(drag_underline_->parent()->children());
}

std::tuple<views::View*, views::View*>
TabGroupViews::GetLeadingTrailingGroupViews(
    std::vector<raw_ptr<views::View, VectorExperimental>> children) const {
  // Elements of |children| may be in different coordinate spaces. Canonicalize
  // to widget space for comparison, since they will be in the same widget.
  views::View* leading_child = nullptr;
  gfx::Rect leading_child_widget_bounds;

  views::View* trailing_child = nullptr;
  gfx::Rect trailing_child_widget_bounds;

  for (views::View* child : children) {
    TabSlotView* tab_slot_view = views::AsViewClass<TabSlotView>(child);
    if (!tab_slot_view || tab_slot_view->group() != group_ ||
        !tab_slot_view->GetVisible())
      continue;

    gfx::Rect child_widget_bounds =
        child->ConvertRectToWidget(child->GetLocalBounds());

    if (!leading_child ||
        child_widget_bounds.x() < leading_child_widget_bounds.x()) {
      leading_child = child;
      leading_child_widget_bounds = child_widget_bounds;
    }

    if (!trailing_child ||
        child_widget_bounds.right() > trailing_child_widget_bounds.right()) {
      trailing_child = child;
      trailing_child_widget_bounds = child_widget_bounds;
    }
  }

  return {leading_child, trailing_child};
}
