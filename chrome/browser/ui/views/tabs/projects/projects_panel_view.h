// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

namespace contextual_tasks {
struct Thread;
}  // namespace contextual_tasks

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class ActionViewController;
}  // namespace views

class Profile;
class ProjectsPanelController;
class ProjectsPanelStateController;
class ProjectsPanelTabGroupsView;

// Parent view of the Projects Panel - holds together the views
// hierarchy including Tab Groups and AI threads.
class ProjectsPanelView : public views::View, gfx::AnimationDelegate {
  METADATA_HEADER(ProjectsPanelView, views::View)

 public:
  explicit ProjectsPanelView(actions::ActionItem* root_action_item,
                             Profile* profile);
  ProjectsPanelView(const ProjectsPanelView&) = delete;
  ProjectsPanelView& operator=(const ProjectsPanelView&) = delete;
  ~ProjectsPanelView() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the projects panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnProjectsPanelStateChanged(
      ProjectsPanelStateController* state_controller);

  double GetResizeAnimationValue() const;

  // views::View:
  void Layout(PassKey) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<views::View> content_container_ = nullptr;
  raw_ptr<ProjectsPanelControlsView> controls_view_ = nullptr;
  raw_ptr<ProjectsPanelTabGroupsView> tab_groups_view_ = nullptr;
  raw_ptr<views::ScrollView> threads_scroll_view_ = nullptr;

  // TODO(crbug.com/475300882): Remove once we fetch thread data from the
  // controller.
  const std::vector<contextual_tasks::Thread> threads_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<ProjectsPanelController> panel_controller_;

  // Animation when opening and closing the panel.
  gfx::SlideAnimation resize_animation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
