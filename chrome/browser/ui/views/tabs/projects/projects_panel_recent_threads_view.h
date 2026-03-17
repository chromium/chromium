// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}

namespace views {
class BoxLayout;
}

class ProjectsPanelRecentThreadsView : public views::View,
                                       public gfx::AnimationDelegate {
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

  // Sets whether the list is expanded to show all threads.
  void SetExpanded(bool expanded);
  bool expanded() { return expanded_; }

  // Applies an inside border inset to the list.
  void SetInsideBorderInsets(const gfx::Insets& insets);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  const std::vector<ProjectsPanelThreadItemView*> item_views_for_testing() {
    return item_views_;
  }

  static void disable_animations_for_testing();

 private:
  void UpdateDisplayedThreads();

  raw_ptr<views::BoxLayout> layout_ = nullptr;

  std::vector<contextual_tasks::Thread> threads_;
  bool expanded_ = false;
  std::vector<ProjectsPanelThreadItemView*> item_views_;
  ThreadPressedCallback thread_button_callback_;

  gfx::SlideAnimation expansion_animation_{this};
  int start_height_ = 0;
  int target_height_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_RECENT_THREADS_VIEW_H_
