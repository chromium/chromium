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

  const bool size_bounded = size_bounds.is_fully_bounded();
  int remaining_width =
      size_bounded ? (size_bounds.width().value() - interior_margin_.width())
                   : 0;
  bool has_found_button_forced_visible = false;
  int mid_priority_width = 0;
  bool divider_space_preallocated = false;
  bool divider_included_for_mid_priority = false;

  // 1. Allocate space for forced visible views and calculate additional space
  // needed for medium priority views.
  if (size_bounded) {
    for (views::View* child : host_view()->children()) {
      if (!IsChildIncludedInLayout(child)) {
        continue;
      }
      const PinnedToolbarActionFlexPriority priority =
          static_cast<PinnedToolbarActionFlexPriority>(
              child->GetProperty(kToolbarButtonFlexPriorityKey));
      gfx::Size preferred_size = child->GetPreferredSize();
      preferred_size.Enlarge(child->GetProperty(views::kMarginsKey)->width(),
                             0);

      // Include forced visible views.
      if (priority == PinnedToolbarActionFlexPriority::kHigh) {
        has_found_button_forced_visible = true;
        remaining_width -= preferred_size.width();
      }
      // Track the total width of items with medium flex priority.
      if (views::Button::AsButton(child) &&
          priority == PinnedToolbarActionFlexPriority::kMedium) {
        mid_priority_width += preferred_size.width();
      }
      // Include the divider if any previous buttons are forced visible.
      if (!views::Button::AsButton(child)) {
        if (has_found_button_forced_visible) {
          divider_space_preallocated = true;
          remaining_width -= preferred_size.width();
        } else if (mid_priority_width > 0) {
          divider_included_for_mid_priority = true;
          mid_priority_width += preferred_size.width();
        }
      }
    }
  }

  // 2. Check if all medium priority items will fit in the remaining space. If
  // so, allocate space for them in the remaining width.
  const bool fits_all_medium_priority =
      !size_bounded || (remaining_width - mid_priority_width > 0);
  divider_space_preallocated =
      divider_space_preallocated ||
      (fits_all_medium_priority && divider_included_for_mid_priority);
  if (size_bounded && fits_all_medium_priority) {
    remaining_width -= mid_priority_width;
  }

  // 3. Allocate space for remaining views, and get their indices. The divider
  // should only be included if buttons after it should be included.
  std::vector<size_t> visible_child_indices;
  size_t index = host_view()->children().size() - 1;
  size_t divider_index = 0;
  int divider_width = 0;
  for (auto i = host_view()->children().rbegin();
       i != host_view()->children().rend(); i++) {
    if (!IsChildIncludedInLayout(*i)) {
      index--;
      continue;
    }
    // If the next child is the divider, skip it. It only is included in the
    // layout if one of the following children is visible.
    if (!views::Button::AsButton(*i)) {
      divider_index = index;
      divider_width = (*i)->GetPreferredSize().width() +
                      (*i)->GetProperty(views::kMarginsKey)->width();
      index--;
      continue;
    }
    // Get the preferred size and include the divider width if this view is
    // causing the divider to need to be shown.
    const int margin_width = (*i)->GetProperty(views::kMarginsKey)->width();
    gfx::Size preferred_size = (*i)->GetPreferredSize();
    preferred_size.Enlarge(margin_width, 0);
    if (divider_index > index && !divider_space_preallocated) {
      preferred_size.Enlarge(divider_width, 0);
    }

    const PinnedToolbarActionFlexPriority priority =
        static_cast<PinnedToolbarActionFlexPriority>(
            (*i)->GetProperty(kToolbarButtonFlexPriorityKey));
    const bool has_preallocated_space_in_layout =
        priority == PinnedToolbarActionFlexPriority::kHigh ||
        (fits_all_medium_priority &&
         priority == PinnedToolbarActionFlexPriority::kMedium);
    const bool fits_in_remaining_space =
        remaining_width >= preferred_size.width();
    const bool should_take_remaining_space =
        fits_all_medium_priority ||
        priority == PinnedToolbarActionFlexPriority::kMedium;

    // Determine whether the child should be visible in the layout and add it to
    // |visible_child_indices| if so.
    if (has_preallocated_space_in_layout ||
        (!size_bounded ||
         (fits_in_remaining_space && should_take_remaining_space))) {
      // Include the divider if necessary.
      if (divider_index != 0) {
        visible_child_indices.push_back(divider_index);
        divider_index = 0;
      }
      if (has_preallocated_space_in_layout) {
        visible_child_indices.push_back(index);
      } else {
        visible_child_indices.push_back(index);
        if (size_bounded) {
          remaining_width -= preferred_size.width();
        }
      }
    }
    index--;
  }

  // 4. Create layout based on prior calculations.
  int total_width = interior_margin_.width();
  int left = interior_margin_.left();
  index = 0;
  const int height = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  for (auto view : host_view()->children()) {
    if (visible_child_indices.size() && index == visible_child_indices.back()) {
      const gfx::Size preferred_size = view->GetPreferredSize();
      const int y = (height - preferred_size.height()) / 2;
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
