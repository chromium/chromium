// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_LAYOUT_H_

#include "ui/views/layout/layout_manager_base.h"

// Custom layout manager for the project panel that handles tab groups and
// threads section sizing as follows:
// - If the combined preferred height of tab groups and threads is shorter than
//   the panel height, lay them out at their preferred heights.
// - If the combined preferred height is taller than the panel height, shrink
//   the taller section by the overflow height amount.
//   - If this causes the taller section to become shorter than the shorter
//     section, split the height evenly between both sections.
class ProjectsPanelViewLayout : public views::LayoutManagerBase {
 public:
  ProjectsPanelViewLayout(views::View* controls_view,
                          views::View* tab_groups_container,
                          views::View* threads_container,
                          views::View* separator_view);
  ~ProjectsPanelViewLayout() override;

  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  raw_ptr<views::View> controls_view_ = nullptr;
  raw_ptr<views::View> tab_groups_container_ = nullptr;
  raw_ptr<views::View> threads_container_ = nullptr;
  raw_ptr<views::View> separator_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_LAYOUT_H_
