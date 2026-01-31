// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsItemView : public views::Button {
  METADATA_HEADER(ProjectsPanelTabGroupsItemView, views::Button)

 public:
  explicit ProjectsPanelTabGroupsItemView(
      const tab_groups::SavedTabGroup& group);
  ProjectsPanelTabGroupsItemView(const ProjectsPanelTabGroupsItemView&) =
      delete;
  ProjectsPanelTabGroupsItemView& operator=(
      const ProjectsPanelTabGroupsItemView&) = delete;
  ~ProjectsPanelTabGroupsItemView() override;

  // views::View
  void OnThemeChanged() override;

  views::Label* title_for_testing() { return title_; }

  const gfx::VectorIcon& tab_group_vector_icon_for_testing() {
    return *tab_group_vector_icon_;
  }

 private:
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> tab_group_icon_ = nullptr;
  const tab_groups::TabGroupColorId tab_group_color_id_;
  raw_ref<const gfx::VectorIcon> tab_group_vector_icon_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_ITEM_VIEW_H_
