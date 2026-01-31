// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PANEL_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PANEL_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}  // namespace actions

class ProjectsPanelStateController {
 public:
  DECLARE_USER_DATA(ProjectsPanelStateController);

  explicit ProjectsPanelStateController(BrowserWindowInterface* browser_window,
                                        actions::ActionItem* root_action_item);
  ProjectsPanelStateController(const ProjectsPanelStateController&) = delete;
  ProjectsPanelStateController& operator=(const ProjectsPanelStateController&) =
      delete;
  ~ProjectsPanelStateController();

  static ProjectsPanelStateController* From(
      BrowserWindowInterface* browser_window);

  bool IsProjectsPanelVisible() const;
  void SetProjectsVisible(bool visible);

  using StateChangedCallback =
      base::RepeatingCallback<void(ProjectsPanelStateController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

 private:
  // Notifies subscribers when the is_visible_ state of the Projects Panel
  // changes.
  void NotifyStateChanged();

  // Update the Project Button's Action Item (kActionToggleProjectsPanel) based
  // on the Project Panel's is_visible_ state.
  void UpdateProjectsActionItem();

  // Controls whether the ProjectPanelView is visible.
  bool is_visible_ = false;

  const raw_ptr<actions::ActionItem> root_action_item_;

  // Callback list for state changes to the visibility.
  base::RepeatingCallbackList<void(ProjectsPanelStateController*)>
      on_state_changed_callback_list_;
  ui::ScopedUnownedUserData<ProjectsPanelStateController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PANEL_STATE_CONTROLLER_H_
