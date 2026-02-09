// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_NO_TAB_GROUPS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_NO_TAB_GROUPS_VIEW_H_

#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

// View to be displayed in Tab groups if there are no tab groups
class ProjectsPanelNoTabGroupsView : public views::View {
  METADATA_HEADER(ProjectsPanelNoTabGroupsView, views::View)

 public:
  explicit ProjectsPanelNoTabGroupsView();
  ProjectsPanelNoTabGroupsView(const ProjectsPanelNoTabGroupsView&) = delete;
  ProjectsPanelNoTabGroupsView& operator=(const ProjectsPanelNoTabGroupsView&) =
      delete;
  ~ProjectsPanelNoTabGroupsView() override;

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  raw_ptr<views::ImageView> no_tab_groups_image_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_NO_TAB_GROUPS_VIEW_H_
