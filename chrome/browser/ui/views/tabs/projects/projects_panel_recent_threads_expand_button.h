// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_EXPAND_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_EXPAND_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

class ProjectsPanelRecentThreadsExpandButton : public views::Button {
  METADATA_HEADER(ProjectsPanelRecentThreadsExpandButton, views::Button)

 public:
  explicit ProjectsPanelRecentThreadsExpandButton(
      base::RepeatingClosure callback);
  ProjectsPanelRecentThreadsExpandButton(
      const ProjectsPanelRecentThreadsExpandButton&) = delete;
  ProjectsPanelRecentThreadsExpandButton& operator=(
      const ProjectsPanelRecentThreadsExpandButton&) = delete;
  ~ProjectsPanelRecentThreadsExpandButton() override;

  // Sets whether the list this button corresponds to is expanded, updating the
  // title and icon.
  void SetExpanded(bool expanded);

  views::Label* title_label_for_testing() { return title_; }
  views::ImageView* icon_view_for_testing() { return icon_; }

 private:
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_EXPAND_BUTTON_H_
