// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}

// Displays a button for a single thread (e.g., a Gemini or AIM thread).
class ProjectsPanelThreadItemView : public views::Button {
  METADATA_HEADER(ProjectsPanelThreadItemView, views::Button)

 public:
  explicit ProjectsPanelThreadItemView(const contextual_tasks::Thread& task);
  ProjectsPanelThreadItemView(const ProjectsPanelThreadItemView&) = delete;
  ProjectsPanelThreadItemView& operator=(const ProjectsPanelThreadItemView&) =
      delete;
  ~ProjectsPanelThreadItemView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_THREAD_ITEM_VIEW_H_
