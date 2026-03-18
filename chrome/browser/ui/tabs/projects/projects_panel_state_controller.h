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

namespace glic {
class GlicEnabling;
}

class AimEligibilityService;

class ProjectsPanelStateController {
 public:
  DECLARE_USER_DATA(ProjectsPanelStateController);

  explicit ProjectsPanelStateController(
      BrowserWindowInterface* browser_window,
      actions::ActionItem* root_action_item,
      AimEligibilityService* aim_eligibility_service,
      glic::GlicEnabling* glic_enabling);
  ProjectsPanelStateController(const ProjectsPanelStateController&) = delete;
  ProjectsPanelStateController& operator=(const ProjectsPanelStateController&) =
      delete;
  virtual ~ProjectsPanelStateController();

  static ProjectsPanelStateController* From(
      BrowserWindowInterface* browser_window);

  bool IsProjectsPanelVisible() const;

  void SetProjectsVisible(bool visible);

  // Whether the Projects Panel can show AI Mode or Gemini threads. This is
  // tracked within the state controller so callers outside of the panel (like
  // IPH triggering sites) can determine what's shown in the panel.
  virtual bool CanShowAimThreads();
  virtual bool CanShowGeminiThreads();

  using StateChangedCallback =
      base::RepeatingCallback<void(ProjectsPanelStateController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

  using ThreadEligibilityChangedCallback =
      base::RepeatingCallback<void(ProjectsPanelStateController*)>;
  base::CallbackListSubscription RegisterOnThreadEligibilityChanged(
      ThreadEligibilityChangedCallback callback);

 private:
  // Notifies subscribers when the is_visible_ state of the Projects Panel
  // changes.
  void NotifyStateChanged();

  // Update the Project Button's Action Item (kActionToggleProjectsPanel) based
  // on the Project Panel's is_visible_ state.
  void UpdateProjectsActionItem();

  // Notifies subscribers when AIM/Gemini thread eligibility changes.
  void NotifyThreadEligibilityChanged();

  // Called when either AI Mode or Gemini eligibility changes.
  void OnAimEligibilityChanged();
  void OnGeminiEligibilityChanged();

  // Controls whether the ProjectPanelView is visible.
  bool is_visible_ = false;

  bool can_show_aim_threads_ = false;
  bool can_show_gemini_threads_ = false;

  base::CallbackListSubscription aim_eligibility_changed_subcription_;
  base::CallbackListSubscription gemini_eligibility_changed_subcription_;

  const raw_ptr<actions::ActionItem> root_action_item_;
  const raw_ptr<AimEligibilityService> aim_eligibility_service_;
  const raw_ptr<glic::GlicEnabling> glic_enabling_;

  // Callback list for state changes to the visibility.
  base::RepeatingCallbackList<void(ProjectsPanelStateController*)>
      on_state_changed_callback_list_;
  // Callback list for state changes to AIM or Gemini eligibility.
  base::RepeatingCallbackList<void(ProjectsPanelStateController*)>
      on_thread_eligibility_changed_callback_list_;
  ui::ScopedUnownedUserData<ProjectsPanelStateController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<ProjectsPanelStateController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PANEL_STATE_CONTROLLER_H_
