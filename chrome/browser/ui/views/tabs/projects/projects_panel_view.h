// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_drag_scroll_handler.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace tab_groups {
class STGTabsMenuModel;
}

namespace views {
class ActionViewController;
class EventMonitor;
class MenuButton;
class MenuRunner;
class ScrollView;
class ViewShadow;
}  // namespace views

class BrowserWindowInterface;

class ProjectsPanelController;
class ProjectsPanelRecentThreadsExpandButton;
class ProjectsPanelRecentThreadsView;
class ProjectsPanelStateController;
class ProjectsPanelTabGroupsView;

// Parent view of the Projects Panel - holds together the views
// hierarchy including Tab Groups and AI threads.
class ProjectsPanelView : public views::View,
                          public ui::SimpleMenuModel::Delegate,
                          public views::FocusChangeListener,
                          public views::FocusTraversable,
                          public gfx::AnimationDelegate,
                          ProjectsPanelController::Observer {
  METADATA_HEADER(ProjectsPanelView, views::View)

 public:
  ProjectsPanelView(BrowserWindowInterface* browser,
                    actions::ActionItem* root_action_item,
                    ProjectsPanelStateController* state_controller);
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
  void RemovedFromWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  views::FocusTraversable* GetPaneFocusTraversable() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

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

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  views::View* content_container_for_testing() { return content_container_; }
  views::View* threads_container_for_testing() { return threads_container_; }
  views::Separator* separator_for_testing() { return separator_; }
  views::Button* create_new_tab_group_button_for_testing() {
    return create_new_tab_group_button_;
  }
  ProjectsPanelControlsView* controls_view_for_testing() {
    return controls_view_;
  }

  void set_on_close_animation_ended_callback_for_testing(
      base::OnceClosure on_close_animation_ended_callback) {
    on_close_animation_ended_callback_ =
        std::move(on_close_animation_ended_callback);
  }

  static void set_threads_visible_for_testing(bool visible);
  static void disable_animations_for_testing();

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

  void ClosePanel(bool caused_by_focus_lost = false);

  void OnTabGroupButtonPressed(const base::Uuid& group_guid);
  void OnTabGroupMoreButtonPressed(const base::Uuid& group_guid,
                                   views::MenuButton& button);
  void OnThreadsActivityMenuButtonPressed();
  void OnTabGroupMoved(const base::Uuid& group_guid, int new_index);
  void OnCreateNewTabGroupButtonPressed();
  void OnThreadButtonPressed(const std::string& thread_server_id,
                             contextual_tasks::ThreadType thread_type);
  void OnTabGroupDragUpdated(const gfx::Point& location);
  void OnTabGroupDragExited();
  void OnThreadExpandButtonPressed();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;

  raw_ptr<views::View> content_container_ = nullptr;
  raw_ptr<ProjectsPanelControlsView> controls_view_ = nullptr;
  raw_ptr<views::View> tab_groups_container_ = nullptr;
  raw_ptr<views::ScrollView> tab_groups_scroll_view_ = nullptr;
  raw_ptr<ProjectsPanelTabGroupsView> tab_groups_view_ = nullptr;
  raw_ptr<views::View> threads_container_ = nullptr;
  raw_ptr<ProjectsPanelRecentThreadsView> threads_view_ = nullptr;
  raw_ptr<ProjectsPanelRecentThreadsExpandButton> threads_expand_button_ =
      nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;
  raw_ptr<views::MenuButton> threads_activity_menu_button_ = nullptr;
  raw_ptr<views::Button> create_new_tab_group_button_ = nullptr;

  std::unique_ptr<views::ViewShadow> content_shadow_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<ProjectsPanelController> panel_controller_;
  const raw_ptr<ProjectsPanelStateController> state_controller_ = nullptr;

  // Animation when opening and closing the panel.
  gfx::SlideAnimation resize_animation_;
  base::OnceClosure on_close_animation_ended_callback_;

  // Handle mouse presses outside the panel.
  MouseEventHandler mouse_event_handler_{this};
  std::unique_ptr<views::EventMonitor> event_monitor_;

  std::unique_ptr<tab_groups::STGTabsMenuModel> tab_group_menu_model_;
  std::unique_ptr<views::MenuRunner> tab_group_menu_runner_;

  std::unique_ptr<ui::SimpleMenuModel> threads_activity_menu_model_;
  std::unique_ptr<views::MenuRunner> threads_activity_menu_runner_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // The target width of the panel when expanded. Used when vertical tabs is
  // enabled since the panel width needs to match when expanded.
  int target_width_ = projects_panel::kProjectsPanelMinWidth;

  // Whether the panel should show with an elevation shadow and rounded borders.
  // The default appearance of the panel is elevated, but this must be false
  // for the SetIsElevated call in the constructor to be effective.
  bool elevated_ = false;

  // Handles auto-scrolling the tab groups view as a group is held near the top
  // or bottom of the list.
  ProjectsPanelTabGroupsDragScrollHandler tab_groups_drag_scroll_handler_;

  // Prevents attempting to (un)observe the focus manager more than once if
  // OnProjectsPanelStateChanged is called twice with the same visibility value.
  bool observing_focus_manager_ = false;

  // Records the last time the panel was opened. Used for recording how long the
  // panel was open.
  base::TimeTicks last_opened_time_;

  // Tracks the last focused view before opening the panel, so focus can be
  // restored when the panel is closed.
  views::ViewTracker last_focused_view_before_opening_;

  base::ScopedObservation<ProjectsPanelController,
                          ProjectsPanelController::Observer>
      panel_controller_observer_{this};

  base::WeakPtrFactory<ProjectsPanelView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
