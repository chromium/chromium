// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_page_action_controller.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(ContextualTasksPageActionController);

ContextualTasksPageActionController::ContextualTasksPageActionController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(tab_interface),
      scoped_unowned_user_data_(tab_interface->GetUnownedUserDataHost(),
                                *this) {
  Profile* const profile =
      tab_interface->GetBrowserWindowInterface()->GetProfile();
  contextual_tasks::ContextualTasksContextController* const context_controller =
      contextual_tasks::ContextualTasksContextControllerFactory::GetForProfile(
          profile);
  contextual_task_observation_.Observe(context_controller);
}

ContextualTasksPageActionController::~ContextualTasksPageActionController() =
    default;

// static:
ContextualTasksPageActionController* ContextualTasksPageActionController::From(
    tabs::TabInterface* tab_interface) {
  return Get(tab_interface->GetUnownedUserDataHost());
}

void ContextualTasksPageActionController::OnTaskAdded(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  UpdatePageActionVisibility();
}

void ContextualTasksPageActionController::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  OnTaskAdded(task, source);
}

void ContextualTasksPageActionController::OnTaskRemoved(
    const base::Uuid& task_id,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  UpdatePageActionVisibility();
}

void ContextualTasksPageActionController::OnWillBeDestroyed() {
  tab_interface_->GetTabFeatures()->page_action_controller()->Hide(
      kActionSidePanelShowContextualTasks);
  contextual_task_observation_.Reset();
}

void ContextualTasksPageActionController::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  UpdatePageActionVisibility();
}

void ContextualTasksPageActionController::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  UpdatePageActionVisibility();
}

void ContextualTasksPageActionController::UpdatePageActionVisibility() {
  Profile* const profile =
      tab_interface_->GetBrowserWindowInterface()->GetProfile();
  contextual_tasks::ContextualTasksContextController* const context_controller =
      contextual_tasks::ContextualTasksContextControllerFactory::GetForProfile(
          profile);
  const SessionID tab_id =
      sessions::SessionTabHelper::IdForTab(tab_interface_->GetContents());
  page_actions::PageActionController* const page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  if (context_controller->GetContextualTaskForTab(tab_id).has_value()) {
    page_action_controller->Show(kActionSidePanelShowContextualTasks);
  } else {
    page_action_controller->Hide(kActionSidePanelShowContextualTasks);
  }
}
