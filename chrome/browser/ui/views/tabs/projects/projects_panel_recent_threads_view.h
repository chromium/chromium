// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_

#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}

class ProjectsPanelRecentThreadsView : public views::View {
  METADATA_HEADER(ProjectsPanelRecentThreadsView, views::View)

 public:
  explicit ProjectsPanelRecentThreadsView(
      const std::vector<contextual_tasks::Thread>& thread);
  ProjectsPanelRecentThreadsView(const ProjectsPanelRecentThreadsView&) =
      delete;
  ProjectsPanelRecentThreadsView& operator=(
      const ProjectsPanelRecentThreadsView&) = delete;
  ~ProjectsPanelRecentThreadsView() override;

  // Updates the threads shown in the list.
  void SetThreads(const std::vector<contextual_tasks::Thread>& threads);

 private:
  // Updates the list UI with the current threads.
  void UpdateListUi();

  raw_ref<const std::vector<contextual_tasks::Thread>> threads_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
