// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_view_layout.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view_utils.h"

namespace {

// The pinned container main axis size should not be larger than half the
// available space unless the unpinned container will not fill that space.
// When pinned tabs are present, the size must be at least as large as the min
// size.
int CalculatePinnedContainerMainAxisSize(int pinned_preferred_size,
                                         int unpinned_preferred_size,
                                         int available_size,
                                         int min_pinned_size) {
  if (pinned_preferred_size == 0) {
    return 0;
  }
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int effective_unpinned_size =
      unpinned_preferred_size > 0
          ? std::max(0, unpinned_preferred_size - tab_overlap)
          : 0;
  const int target_size = std::min(
      pinned_preferred_size,
      std::max(available_size - effective_unpinned_size, available_size / 2));
  return std::max(target_size, min_pinned_size);
}

}  // namespace

TabStripViewLayout::TabStripViewLayout(TabStripOrientation orientation)
    : orientation_(orientation) {}

TabStripViewLayout::~TabStripViewLayout() = default;

views::ProposedLayout TabStripViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const TabStripView* tab_strip_view =
      views::AsViewClass<TabStripView>(host_view());
  if (!tab_strip_view) {
    return views::ProposedLayout();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalLayout(tab_strip_view, size_bounds);
  }
  return CalculateVerticalLayout(tab_strip_view, size_bounds);
}

views::ProposedLayout TabStripViewLayout::CalculateHorizontalLayout(
    const TabStripView* tab_strip_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  views::ScrollView* pinned_tabs_scroll_view =
      tab_strip_view->pinned_tabs_scroll_view();
  views::ScrollView* unpinned_tabs_scroll_view =
      tab_strip_view->unpinned_tabs_scroll_view();
  views::Separator* tabs_separator = tab_strip_view->GetTabsSeparator();
  TabScrollButtonContainer* scroll_button_container =
      tab_strip_view->GetScrollButtonContainer();
  const int scroll_button_container_preferred_width =
      scroll_button_container
          ? scroll_button_container->GetPreferredSize(size_bounds).width()
          : 0;

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  int x = 0;
  const int container_height = TabStyle::Get()->GetStandardHeight();

  const auto* unpinned_container = tab_strip_view->GetUnpinnedTabsContainer();
  const int pinned_preferred_width =
      pinned_tabs_scroll_view->GetPreferredSize(size_bounds).width();
  // Use unconstrained preferred size so the layout accounts for the total
  // desired width of unpinned tabs and groups. The unpinned container may not
  // be set yet so fallback to 0 if it doesn't exist.
  const int unpinned_preferred_width =
      unpinned_container ? unpinned_container->GetUnconstrainedPreferredWidth()
                         : 0;

  const views::SizeBound available_width = size_bounds.width();

  // Place the pinned container.
  int pinned_width = pinned_preferred_width;
  if (available_width.is_bounded()) {
    int min_pinned_width = 0;
    if (const auto* pinned_container =
            tab_strip_view->GetPinnedTabsContainer()) {
      min_pinned_width = pinned_container->GetMinimumSize().width();
    }
    pinned_width = CalculatePinnedContainerMainAxisSize(
        pinned_preferred_width, unpinned_preferred_width,
        available_width.value(), min_pinned_width);
  }

  gfx::Rect pinned_bounds(x, 0, pinned_width, container_height);
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view,
                                     pinned_tabs_scroll_view->GetVisible(),
                                     pinned_bounds);
  const bool has_unpinned = unpinned_preferred_width > 0;

  if (pinned_width > 0) {
    // Unpinned container overlaps with the last pinned tab by tab_overlap.
    x += has_unpinned ? std::max(0, pinned_width - tab_overlap) : pinned_width;
  }

  // The tabs separator isn't visible for the horizontal orientation.
  layouts.child_layouts.emplace_back(tabs_separator, false, gfx::Rect());

  // Place the unpinned container.
  int unpinned_width = unpinned_preferred_width;
  bool show_scroll_buttons = false;

  if (available_width.is_bounded()) {
    int available_unpinned_width = std::max(available_width.value() - x, 0);

    // If placing the unpinned scroll view into the available space causes an
    // overflow, reserve space for the scroll buttons.
    const bool will_overflow_without_scroll_buttons =
        unpinned_container &&
        available_unpinned_width < unpinned_container->GetMinimumSize().width();
    const bool is_scroll_buttons_pinned =
        tab_strip_view->IsTabScrollButtonsPinned();
    if (has_unpinned && will_overflow_without_scroll_buttons &&
        is_scroll_buttons_pinned) {
      available_unpinned_width = std::max(
          available_unpinned_width - scroll_button_container_preferred_width,
          0);
      show_scroll_buttons = true;
    }

    // Do not overwrite available space during zero-size measurement queries.
    if (available_width.value() > 0) {
      tab_strip_view->SetAvailableUnpinnedSpace(
          views::SizeBound(available_unpinned_width));
    }
    unpinned_width = std::min(unpinned_width, available_unpinned_width);
  }
  gfx::Rect unpinned_bounds(x, 0, unpinned_width, container_height);
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view,
                                     unpinned_tabs_scroll_view->GetVisible(),
                                     unpinned_bounds);
  x += unpinned_width;

  if (scroll_button_container) {
    if (show_scroll_buttons) {
      gfx::Rect scroll_container_bounds(
          x, 0, scroll_button_container_preferred_width, container_height);
      layouts.child_layouts.emplace_back(scroll_button_container, true,
                                         scroll_container_bounds);
      x += scroll_button_container_preferred_width;
    } else {
      layouts.child_layouts.emplace_back(scroll_button_container, false,
                                         gfx::Rect());
    }
  }

  int total_host_width =
      has_unpinned ? std::max(pinned_width, x) : pinned_width;

  layouts.host_size = gfx::Size(total_host_width, container_height);
  return layouts;
}

views::ProposedLayout TabStripViewLayout::CalculateVerticalLayout(
    const TabStripView* tab_strip_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  views::ScrollView* pinned_tabs_scroll_view =
      tab_strip_view->pinned_tabs_scroll_view();
  views::ScrollView* unpinned_tabs_scroll_view =
      tab_strip_view->unpinned_tabs_scroll_view();
  views::Separator* tabs_separator = tab_strip_view->GetTabsSeparator();
  const bool is_collapsed = tab_strip_view->is_collapsed_;

  if (!size_bounds.width().is_bounded()) {
    return layouts;
  }

  const int region_horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  const int region_vertical_padding = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);

  int y = 0;

  // Determine container preferred heights.
  views::SizeBounds pinned_tab_container_size_bounds =
      size_bounds.Inset(gfx::Insets::TLBR(0, region_horizontal_padding, 0, 0));
  const int pinned_preferred_height =
      pinned_tabs_scroll_view
          ->GetPreferredSize(pinned_tab_container_size_bounds)
          .height();
  const int unpinned_preferred_height =
      unpinned_tabs_scroll_view->GetPreferredSize(size_bounds).height();

  const bool should_show_separator = pinned_preferred_height != 0 &&
                                     unpinned_preferred_height != 0 &&
                                     is_collapsed;

  // If the height is bounded, calculate the available space for laying out the
  // pinned and unpinned containers.
  int remaining_height = 0;
  if (size_bounds.height().is_bounded()) {
    remaining_height = size_bounds.height().value();
    if (pinned_preferred_height != 0 && unpinned_preferred_height != 0) {
      remaining_height -= region_vertical_padding;
    }
    if (should_show_separator) {
      remaining_height -=
          tabs_separator->GetPreferredSize().height() + region_vertical_padding;
    }
    // Clamp the remaining height to 0 if we have less space.
    remaining_height = std::max(remaining_height, 0);
  }

  // Place the pinned container.
  int pinned_container_height = pinned_preferred_height;
  if (size_bounds.height().is_bounded()) {
    int min_pinned_height = 0;
    if (const auto* pinned_container =
            tab_strip_view->GetPinnedTabsContainer()) {
      min_pinned_height = pinned_container->GetMinimumSize().height();
    }
    pinned_container_height = CalculatePinnedContainerMainAxisSize(
        pinned_preferred_height, unpinned_preferred_height, remaining_height,
        min_pinned_height);
    remaining_height -= pinned_container_height;
  }
  gfx::Rect pinned_container_bounds(
      region_horizontal_padding, y,
      pinned_tab_container_size_bounds.width().value(),
      pinned_container_height);
  layouts.child_layouts.emplace_back(pinned_tabs_scroll_view,
                                     pinned_tabs_scroll_view->GetVisible(),
                                     pinned_container_bounds);

  if (pinned_container_bounds.height()) {
    y += pinned_container_bounds.height();
    // Add padding only if there are pinned and unpinned tabs.
    if (unpinned_preferred_height != 0) {
      y += region_vertical_padding;
    }
  }

  // Place the tabs separator if visible.
  gfx::Rect separator_bounds;
  if (should_show_separator) {
    int separator_width =
        size_bounds.width().value() - 2 * region_horizontal_padding;
    int separator_x = region_horizontal_padding;
    separator_bounds = gfx::Rect(separator_x, y, separator_width,
                                 tabs_separator->GetPreferredSize().height());
    y += separator_bounds.height() + region_vertical_padding;
  }
  layouts.child_layouts.emplace_back(tabs_separator, should_show_separator,
                                     separator_bounds);

  // Place the unpinned container using the entire available width, we do not
  // inset the x value by `region_horizontal_padding` here because, when the tab
  // strip is collapsed, tab groups need to draw the group colored line in this
  // space.
  gfx::Rect unpinned_container_bounds(0, y, size_bounds.width().value(),
                                      unpinned_preferred_height);
  if (size_bounds.height().is_bounded()) {
    int min_unpinned_height = 0;
    if (const auto* unpinned_container =
            tab_strip_view->GetUnpinnedTabsContainer()) {
      min_unpinned_height = unpinned_container->GetMinimumSize().height();
    }
    unpinned_container_bounds.set_height(
        std::max(std::min(unpinned_container_bounds.height(), remaining_height),
                 min_unpinned_height));
  }
  layouts.child_layouts.emplace_back(unpinned_tabs_scroll_view,
                                     unpinned_tabs_scroll_view->GetVisible(),
                                     unpinned_container_bounds);

  layouts.host_size = gfx::Size(size_bounds.width().value(),
                                unpinned_container_bounds.bottom());
  return layouts;
}
