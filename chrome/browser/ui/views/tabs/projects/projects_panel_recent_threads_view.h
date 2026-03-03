// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}

class ProjectsPanelRecentThreadsView : public views::View {
  METADATA_HEADER(ProjectsPanelRecentThreadsView, views::View)

 public:
  using ThreadPressedCallback =
      ProjectsPanelThreadItemView::ThreadPressedCallback;

  explicit ProjectsPanelRecentThreadsView(
      ThreadPressedCallback thread_button_callback = base::DoNothing());
  ProjectsPanelRecentThreadsView(const ProjectsPanelRecentThreadsView&) =
      delete;
  ProjectsPanelRecentThreadsView& operator=(
      const ProjectsPanelRecentThreadsView&) = delete;
  ~ProjectsPanelRecentThreadsView() override;

  // Updates the threads shown in the list.
  void SetThreads(const std::vector<contextual_tasks::Thread>& threads);

  const std::vector<ProjectsPanelThreadItemView*> item_views_for_testing() {
    return item_views_;
  }

 private:
  std::vector<ProjectsPanelThreadItemView*> item_views_;
  ThreadPressedCallback thread_button_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
