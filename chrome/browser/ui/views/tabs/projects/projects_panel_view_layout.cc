// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view_layout.h"

#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"

ProjectsPanelViewLayout::ProjectsPanelViewLayout(
    views::View* controls_view,
    views::View* tab_groups_container,
    views::View* threads_container,
    views::View* separator_view)
    : controls_view_(controls_view),
      tab_groups_container_(tab_groups_container),
      threads_container_(threads_container),
      separator_view_(separator_view) {}

ProjectsPanelViewLayout::~ProjectsPanelViewLayout() = default;

views::ProposedLayout ProjectsPanelViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;

  // Determine the host size.
  int host_width =
      size_bounds.width().value_or(projects_panel::kProjectsPanelMinWidth);
  int host_height = size_bounds.height().value_or(0);

  // Fallback for unbounded height (e.g. during initial preferred size
  // calculations)
  if (!size_bounds.height().is_bounded()) {
    int pref_height =
        projects_panel::kProjectsPanelRegionInteriorMargins.height();
    for (views::View* child : host_view()->children()) {
      if (!child->GetVisible()) {
        continue;
      }
      if (child == separator_view_) {
        pref_height += child->GetPreferredSize().height() +
                       projects_panel::kListsSeparatorMargins.height();
      } else {
        pref_height += child->GetPreferredSize().height();
      }
    }
    host_height = pref_height;
  }

  layout.host_size = gfx::Size(host_width, host_height);

  // Initial positioning constraints.
  int x = projects_panel::kProjectsPanelRegionInteriorMargins.left();
  int y = projects_panel::kProjectsPanelRegionInteriorMargins.top();
  int width = std::max(
      0,
      host_width - projects_panel::kProjectsPanelRegionInteriorMargins.width());

  auto place_child = [&](views::View* child, int height) {
    if (!child || !child->GetVisible()) {
      return;
    }
    layout.child_layouts.emplace_back(child, child->GetVisible(),
                                      gfx::Rect(x, y, width, height));
    y += height;
  };

  // The container views (tab groups and threads) are given the full width of
  // the panel to account for their scroll bars appearing slightly outset from
  // their content. This discrepancy is accounted for in their margins.
  auto place_container_child = [&](views::View* child, int height) {
    if (!child || !child->GetVisible()) {
      return;
    }
    layout.child_layouts.emplace_back(child, child->GetVisible(),
                                      gfx::Rect(0, y, host_width, height));
    y += height;
  };

  // Place the controls view.
  if (controls_view_ && controls_view_->GetVisible()) {
    place_child(controls_view_, controls_view_->GetPreferredSize().height());
  }

  // Calculate the remaining fixed height usage from margins and the list
  // separator.
  int fixed_height =
      y + projects_panel::kProjectsPanelRegionInteriorMargins.bottom();
  if (separator_view_ && separator_view_->GetVisible()) {
    fixed_height += separator_view_->GetPreferredSize().height() +
                    projects_panel::kListsSeparatorMargins.height();
  }

  // Calculate the height available for tab groups and threads.
  int available_height = std::max(0, host_height - fixed_height);
  int tg_height = 0;
  int th_height = 0;

  // Determine the heights for tab groups and threads.
  if (tab_groups_container_ && tab_groups_container_->GetVisible() &&
      threads_container_ && threads_container_->GetVisible()) {
    int pref_height_tg = tab_groups_container_->GetPreferredSize().height();
    int pref_height_th = threads_container_->GetPreferredSize().height();

    // If the combined preferred heights of the tab groups and threads
    // sections are taller than the panel can fit,
    if (pref_height_tg + pref_height_th <= available_height) {
      tg_height = pref_height_tg;
      th_height = pref_height_th;
    } else {
      // Shrink the taller section by the amount over the panel height. If
      // that makes the taller section shorter than the shorter section, split
      // the space evenly.
      int height_overflow =
          (pref_height_tg + pref_height_th) - available_height;
      if (pref_height_tg > pref_height_th &&
          pref_height_tg - height_overflow > pref_height_th) {
        tg_height = pref_height_tg - height_overflow;
        th_height = pref_height_th;
      } else if (pref_height_th > pref_height_tg &&
                 pref_height_th - height_overflow > pref_height_tg) {
        tg_height = pref_height_tg;
        th_height = pref_height_th - height_overflow;
      } else {
        tg_height = available_height / 2;
        th_height = available_height - tg_height;
      }
    }
  } else if (tab_groups_container_ && tab_groups_container_->GetVisible()) {
    tg_height = available_height;
  } else if (threads_container_ && threads_container_->GetVisible()) {
    th_height = available_height;
  }

  // Place the tab groups section.
  place_container_child(tab_groups_container_, tg_height);

  // Place the separator.
  if (separator_view_ && separator_view_->GetVisible()) {
    y += projects_panel::kListsSeparatorMargins.top();
    place_child(separator_view_, separator_view_->GetPreferredSize().height());
    y += projects_panel::kListsSeparatorMargins.bottom();
  }

  // Place the threads section.
  place_container_child(threads_container_, th_height);

  return layout;
}
