// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace tab_groups {
class STGTabsMenuModel;
}

namespace views {
class ActionViewController;
class EventMonitor;
class MenuRunner;
class ViewShadow;
}  // namespace views

class BrowserWindowInterface;

class ProjectsPanelController;
class ProjectsPanelRecentThreadsView;
class ProjectsPanelStateController;
class ProjectsPanelTabGroupsView;

// Parent view of the Projects Panel - holds together the views
// hierarchy including Tab Groups and AI threads.
class ProjectsPanelView : public views::View,
                          gfx::AnimationDelegate,
                          ProjectsPanelController::Observer {
  METADATA_HEADER(ProjectsPanelView, views::View)

 public:
  ProjectsPanelView(BrowserWindowInterface* browser,
                    actions::ActionItem* root_action_item);
  ProjectsPanelView(const ProjectsPanelView&) = delete;
  ProjectsPanelView& operator=(const ProjectsPanelView&) = delete;
  ~ProjectsPanelView() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the projects panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnProjectsPanelStateChanged(
      ProjectsPanelStateController* state_controller);

  double GetResizeAnimationValue() const;

  // Set the width of the panel when it is fully expanded.
  void SetTargetWidth(int target_width);

  // Set whether the panel should appear elevated with rounded borders.
  void SetIsElevated(bool elevated);

  // Whether the panel appears elevated with rounded borders.
  bool is_elevated() { return elevated_; }

  // views::View:
  void Layout(PassKey) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // ProjectsPanelController::Observer:
  void OnTabGroupsInitialized(
      const std::vector<tab_groups::SavedTabGroup>& tab_groups) override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       int index) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id, int old_index) override;
  void OnTabGroupsReordered(
      const std::vector<tab_groups::SavedTabGroup>& tab_groups) override;
  void OnThreadsInitialized(
      const std::vector<contextual_tasks::Thread>& threads) override;

  views::View* content_container_for_testing() { return content_container_; }

  static void disable_animations_for_testing();

  void set_on_close_animation_ended_callback_for_testing(
      base::OnceClosure on_close_animation_ended_callback) {
    on_close_animation_ended_callback_ =
        std::move(on_close_animation_ended_callback);
  }

 private:
  // Detects if mouse presses occur outside of the panel.
  class MouseEventHandler : public ui::EventObserver {
   public:
    explicit MouseEventHandler(ProjectsPanelView* owning_view);
    MouseEventHandler(const MouseEventHandler&) = delete;
    MouseEventHandler& operator=(const MouseEventHandler&) = delete;
    ~MouseEventHandler() override;

    void OnEvent(const ui::Event& event) override;

   private:
    raw_ptr<ProjectsPanelView> owning_view_ = nullptr;
  };

  void ClosePanel();

  void OnTabGroupButtonPressed(const base::Uuid& group_guid);
  void OnTabGroupMoreButtonPressed(const base::Uuid& group_guid,
                                   views::MenuButton& button);
  void OnTabGroupMoved(const base::Uuid& group_guid, int new_index);
  void OnCreateNewTabGroupButtonPressed();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<views::View> content_container_ = nullptr;
  raw_ptr<ProjectsPanelControlsView> controls_view_ = nullptr;
  raw_ptr<ProjectsPanelTabGroupsView> tab_groups_view_ = nullptr;
  raw_ptr<ProjectsPanelRecentThreadsView> threads_view_ = nullptr;

  std::unique_ptr<views::ViewShadow> content_shadow_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<ProjectsPanelController> panel_controller_;

  // Animation when opening and closing the panel.
  gfx::SlideAnimation resize_animation_;
  base::OnceClosure on_close_animation_ended_callback_;

  // Handle mouse presses outside the panel.
  MouseEventHandler mouse_event_handler_{this};
  std::unique_ptr<views::EventMonitor> event_monitor_;

  std::unique_ptr<tab_groups::STGTabsMenuModel> tab_group_menu_model_;
  std::unique_ptr<views::MenuRunner> tab_group_menu_runner_;

  // The target width of the panel when expanded. Used when vertical tabs is
  // enabled since the panel width needs to match when expanded.
  int target_width_ = projects_panel::kProjectsPanelMinWidth;

  // Whether the panel should show with an elevation shadow and rounded borders.
  // The default appearance of the panel is elevated, but this must be false
  // for the SetIsElevated call in the constructor to be effective.
  bool elevated_ = false;

  base::ScopedObservation<ProjectsPanelController,
                          ProjectsPanelController::Observer>
      panel_controller_observer_{this};

  base::WeakPtrFactory<ProjectsPanelView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
