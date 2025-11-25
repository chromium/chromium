// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"

#include <optional>

#include "base/functional/bind.h"
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
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(ContextualTasksEphemeralButtonController);

ContextualTasksEphemeralButtonController::
    ContextualTasksEphemeralButtonController(
        BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  Profile* const profile = browser_window_interface_->GetProfile();
  contextual_tasks::ContextualTasksContextController* const context_controller =
      contextual_tasks::ContextualTasksContextControllerFactory::GetForProfile(
          profile);
  contextual_task_observation_.Observe(context_controller);
  tab_change_subscription_ =
      browser_window_interface_->RegisterActiveTabDidChange(base::BindRepeating(
          &ContextualTasksEphemeralButtonController::OnActiveTabChange,
          base::Unretained(this)));
}

ContextualTasksEphemeralButtonController::
    ~ContextualTasksEphemeralButtonController() = default;

// static:
ContextualTasksEphemeralButtonController*
ContextualTasksEphemeralButtonController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void ContextualTasksEphemeralButtonController::OnTaskAdded(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  SessionID current_tab_session_id = GetCurrentTabSessionId();
  for (SessionID id : task.GetTabIds()) {
    if (current_tab_session_id == id) {
      should_update_visibility_callbacks_.Notify(true);
    }
  }
}

void ContextualTasksEphemeralButtonController::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  OnTaskAdded(task, source);
}

void ContextualTasksEphemeralButtonController::OnTaskRemoved(
    const base::Uuid& task_id,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  should_update_visibility_callbacks_.Notify(false);
}

void ContextualTasksEphemeralButtonController::OnWillBeDestroyed() {
  should_update_visibility_callbacks_.Notify(false);
  contextual_task_observation_.Reset();
}

base::CallbackListSubscription
ContextualTasksEphemeralButtonController::RegisterShouldUpdateButtonVisibility(
    ShouldUpdateVisibilityCallbackList::CallbackType callback) {
  return should_update_visibility_callbacks_.Add(std::move(callback));
}

contextual_tasks::ContextualTasksService*
ContextualTasksEphemeralButtonController::GetContextualTasksService() {
  return contextual_tasks::ContextualTasksContextControllerFactory::
      GetForProfile(browser_window_interface_->GetProfile());
}

SessionID ContextualTasksEphemeralButtonController::GetCurrentTabSessionId() {
  return sessions::SessionTabHelper::IdForTab(
      browser_window_interface_->GetActiveTabInterface()->GetContents());
}

void ContextualTasksEphemeralButtonController::OnActiveTabChange(
    BrowserWindowInterface* browser_window_interface) {
  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          GetCurrentTabSessionId());
  should_update_visibility_callbacks_.Notify(current_task.has_value());
}
