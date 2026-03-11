// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"

#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

ProjectsPanelRecentThreadsView::ProjectsPanelRecentThreadsView(
    ThreadPressedCallback thread_button_callback)
    : thread_button_callback_(std::move(thread_button_callback)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
}

ProjectsPanelRecentThreadsView::~ProjectsPanelRecentThreadsView() = default;

void ProjectsPanelRecentThreadsView::SetThreads(
    const std::vector<contextual_tasks::Thread>& threads) {
  item_views_.clear();
  RemoveAllChildViews();

  size_t count = 0;
  for (const auto& thread : threads) {
    item_views_.push_back(
        AddChildView(std::make_unique<ProjectsPanelThreadItemView>(
            thread, thread_button_callback_)));
    if (++count >= projects_panel::kMaxNumberOfRecentThreads) {
      return;
    }
  }
}

BEGIN_METADATA(ProjectsPanelRecentThreadsView)
END_METADATA
