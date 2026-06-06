// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"

#include <algorithm>
#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/common/url_constants.h"

namespace {
bool IsContextualTasksPage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}
}  // namespace

DEFINE_USER_DATA(ContextualTasksEphemeralButtonController);

ContextualTasksEphemeralButtonController::
    ContextualTasksEphemeralButtonController(
        BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  Profile* const profile = browser_window_interface_->GetProfile();
  contextual_tasks::ContextualTasksService* const contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  contextual_task_observation_.Observe(contextual_tasks_service);
  tab_change_subscription_ =
      browser_window_interface_->RegisterActiveTabDidChange(base::BindRepeating(
          &ContextualTasksEphemeralButtonController::OnActiveTabChange,
          base::Unretained(this)));

  contextual_task_entry_observation_.Observe(
      SidePanelRegistry::From(browser_window_interface_)
          ->GetEntryForKey(
              SidePanelEntryKey(SidePanelEntryId::kContextualTasks)));

  aim_eligibility_service_ =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (aim_eligibility_service_) {
    aim_eligibility_service_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(&ContextualTasksEphemeralButtonController::
                                    OnAimEligibilityResponseChanged,
                                base::Unretained(this)));
  }
  UpdateActiveTabObservation();
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
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  OnTaskAdded(task, source);
}

void ContextualTasksEphemeralButtonController::OnTaskRemoved(
    const base::Uuid& task_id,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  ephemeral_button_eligible_tasks_.erase(
      std::remove(ephemeral_button_eligible_tasks_.begin(),
                  ephemeral_button_eligible_tasks_.end(), task_id),
      ephemeral_button_eligible_tasks_.end());
  should_update_visibility_callbacks_.Notify(false);
}

void ContextualTasksEphemeralButtonController::OnWillBeDestroyed() {
  should_update_visibility_callbacks_.Notify(false);
  contextual_task_observation_.Reset();
  tab_discard_subscription_ = base::CallbackListSubscription();
  Observe(nullptr);
}

void ContextualTasksEphemeralButtonController::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::
    OnAimEligibilityResponseChanged() {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnEntryShown(
    SidePanelEntry* entry) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    is_contextual_tasks_panel_open_ = true;
    is_hiding_contextual_tasks_panel_ = false;
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksEphemeralButtonController::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  is_hiding_contextual_tasks_panel_ = true;
  if (!IsActiveTabAssociatedToTask()) {
    return;
  }

  if (reason == SidePanelEntryHideReason::kBackgrounded) {
    return;
  }

  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          GetCurrentTabSessionId().value());

  ephemeral_button_eligible_tasks_.emplace_back(current_task->GetTaskId());
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnEntryHideCancelled(
    SidePanelEntry* entry) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    is_hiding_contextual_tasks_panel_ = false;
    is_contextual_tasks_panel_open_ = true;
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksEphemeralButtonController::OnEntryHidden(
    SidePanelEntry* entry) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    is_hiding_contextual_tasks_panel_ = false;
    is_contextual_tasks_panel_open_ = false;
    MaybeNotifyVisibilityShouldChange();
  }
}

base::CallbackListSubscription
ContextualTasksEphemeralButtonController::RegisterShouldUpdateButtonVisibility(
    ShouldUpdateVisibilityCallbackList::CallbackType callback) {
  return should_update_visibility_callbacks_.Add(std::move(callback));
}

bool ContextualTasksEphemeralButtonController::ShouldShowEphemeralButton() {
  // TabInterface can be null on browser shutdown.
  tabs::TabInterface* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();

  if (!tab_interface) {
    return false;
  }

  if (contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded &&
      IsContextualTasksPage(tab_interface->GetURL())) {
    return false;
  }

  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          GetCurrentTabSessionId().value());

  if (aim_eligibility_service_ &&
      !aim_eligibility_service_->IsCobrowseEligible()) {
    return false;
  }

  // The ephemeral toolbar button should show if the contextual task side panel
  // was closed.
  bool should_show_button =
      current_task.has_value() &&
      std::ranges::contains(ephemeral_button_eligible_tasks_,
                            current_task->GetTaskId());

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    should_show_button &=
        (!is_contextual_tasks_panel_open_ || is_hiding_contextual_tasks_panel_);
  }

  return should_show_button;
}

contextual_tasks::ContextualTasksService*
ContextualTasksEphemeralButtonController::GetContextualTasksService() {
  return contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile());
}

std::optional<SessionID>
ContextualTasksEphemeralButtonController::GetCurrentTabSessionId() {
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (active_tab) {
    return sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  } else {
    return std::nullopt;
  }
}

bool ContextualTasksEphemeralButtonController::IsActiveTabAssociatedToTask() {
  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  if (!current_tab_session_id.has_value()) {
    return false;
  }

  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          current_tab_session_id.value());
  return current_task.has_value();
}

void ContextualTasksEphemeralButtonController::OnActiveTabChange(
    BrowserWindowInterface* browser_window_interface) {
  UpdateActiveTabObservation();
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::
    MaybeNotifyVisibilityShouldChange() {
  should_update_visibility_callbacks_.Notify(ShouldShowEphemeralButton());
}

void ContextualTasksEphemeralButtonController::PrimaryPageChanged(
    content::Page& page) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksEphemeralButtonController::UpdateActiveTabObservation() {
  if (contextual_tasks::kShowEntryPoint.Get() !=
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return;
  }
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (active_tab) {
    Observe(active_tab->GetContents());
    tab_discard_subscription_ =
        active_tab->RegisterWillDiscardContents(base::BindRepeating(
            &ContextualTasksEphemeralButtonController::OnTabDiscarded,
            base::Unretained(this)));
  } else {
    Observe(nullptr);
    tab_discard_subscription_ = base::CallbackListSubscription();
  }
}

void ContextualTasksEphemeralButtonController::OnTabDiscarded(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
  MaybeNotifyVisibilityShouldChange();
}
