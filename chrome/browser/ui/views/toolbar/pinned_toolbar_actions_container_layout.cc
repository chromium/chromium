// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_class_properties.h"

views::ProposedLayout
PinnedToolbarActionsContainerLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;

  int total_width = interior_margin_.width();
  bool size_bounded = size_bounds.is_fully_bounded();
  int remaining_width =
      size_bounded ? (size_bounds.width().value() - interior_margin_.width())
                   : 0;

  // 1. Allocate space for forced visible views
  for (views::View* child : host_view()->children()) {
    if (child->GetProperty(kToolbarButtonFlexWeightKey) == 0) {
      gfx::Size preferred_size = child->GetPreferredSize();
      preferred_size.Enlarge(child->GetProperty(views::kMarginsKey)->width(),
                             0);
      if (size_bounded) {
        remaining_width -= preferred_size.width();
      }
    }
  }

  // 2. Allocate space for remaining views, the divider should only be
  // included if buttons after it should be included.
  std::vector<size_t> visible_child_indices;
  size_t index = host_view()->children().size() - 1;
  size_t divider_index = 0;
  int divider_width = 0;
  for (auto i = host_view()->children().rbegin();
       i != host_view()->children().rend(); i++) {
    // If the next child is the divider, skip it. It only is included in the
    // layout if one of the following children is visible.
    if (!views::Button::AsButton(*i)) {
      divider_index = index;
      divider_width = (*i)->GetPreferredSize().width() +
                      (*i)->GetProperty(views::kMarginsKey)->width();
      index--;
      continue;
    }
    int margin_width = (*i)->GetProperty(views::kMarginsKey)->width();
    gfx::Size preferred_size = (*i)->GetPreferredSize();
    preferred_size.Enlarge(margin_width, 0);
    if (divider_index > index) {
      preferred_size.set_width(preferred_size.width() + divider_width);
    }
    int flex_weight = (*i)->GetProperty(kToolbarButtonFlexWeightKey);
    if (flex_weight == 0 ||
        (!size_bounded || remaining_width >= preferred_size.width())) {
      // Include the divider if necessary.
      if (divider_index != 0) {
        visible_child_indices.push_back(divider_index);
        if (size_bounded) {
          remaining_width -= divider_width;
        }
        divider_index = 0;
      }
      if (flex_weight == 0) {
        visible_child_indices.push_back(index);
      } else {
        visible_child_indices.push_back(index);
        if (size_bounded) {
          remaining_width -= (*i)->GetPreferredSize().width() + margin_width;
        }
      }
    }
    index--;
  }

  int left = interior_margin_.left();
  index = 0;
  int height = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  for (auto view : host_view()->children()) {
    if (visible_child_indices.size() && index == visible_child_indices.back()) {
      gfx::Size preferred_size = view->GetPreferredSize();
      int y = (height - preferred_size.height()) / 2;
      left += view->GetProperty(views::kMarginsKey)->left();
      layout.child_layouts.emplace_back(views::ChildLayout{
          view, true,
          gfx::Rect(left, y, preferred_size.width(), preferred_size.height())});
      left += preferred_size.width() +
              view->GetProperty(views::kMarginsKey)->right();
      total_width += preferred_size.width() +
                     view->GetProperty(views::kMarginsKey)->width();
      visible_child_indices.pop_back();
    } else {
      layout.child_layouts.emplace_back(views::ChildLayout{view, false});
    }
    index++;
  }
  layout.host_size = gfx::Size(total_width, height);
  return layout;
}

void PinnedToolbarActionsContainerLayout::SetInteriorMargin(
    const gfx::Insets& interior_margin) {
  if (interior_margin_ != interior_margin) {
    interior_margin_ = interior_margin;
    InvalidateHost(true);
  }
}
