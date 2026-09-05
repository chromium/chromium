// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_browser_controller.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "components/contextual_tasks/public/features.h"
#endif

namespace contextual_tasks {

DEFINE_USER_DATA(ContextualTasksBrowserController);

ContextualTasksBrowserController::ContextualTasksBrowserController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  active_task_context_provider_ =
      std::make_unique<ActiveTaskContextProviderImpl>(
          browser_window_interface_,
          ContextualTasksServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile()));

  eligibility_manager_ =
      std::make_unique<EntryPointEligibilityManager>(browser_window_interface_);

  side_panel_coordinator_ =
      std::make_unique<ContextualTasksSidePanelCoordinator>(
          browser_window_interface_,
          ContextualTasksPanelHost::Create(browser_window_interface_),
          active_task_context_provider_.get(), eligibility_manager_.get());

#if !BUILDFLAG(IS_ANDROID)
  if (kShowEntryPoint.Get() == EntryPointOption::kToolbarEphemeralBranded) {
    ephemeral_button_controller_ =
        std::make_unique<ContextualTasksEphemeralButtonController>(
            browser_window_interface_);
  }

  close_button_controller_ =
      std::make_unique<ContextualTasksCloseButtonController>(
          browser_window_interface_, eligibility_manager_.get(),
          side_panel_coordinator_.get());

  UpdatePinButtonVisibilityState(browser_window_interface_);

  // This subscription is safe because the browser_window_interface_
  // BrowserWindowFeatures, which owns this controller and this subscription.
  actions_visibility_subscription_ =
      eligibility_manager_->RegisterOnEntryPointEligibilityChanged(
          base::IgnoreArgs<bool>(
              base::BindRepeating(&UpdatePinButtonVisibilityState,
                                  base::Unretained(browser_window_interface_))));
#endif
}


void ContextualTasksBrowserController::Shutdown() {
#if !BUILDFLAG(IS_ANDROID)
  actions_visibility_subscription_ = base::CallbackListSubscription();

  close_button_controller_.reset();
  ephemeral_button_controller_.reset();
#endif
  if (side_panel_coordinator_) {
    // Tell the side panel coordinator to disconnect its observers safely.
    side_panel_coordinator_.reset();
  }
}

ContextualTasksBrowserController::~ContextualTasksBrowserController() = default;

// static
ContextualTasksBrowserController* ContextualTasksBrowserController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

}  // namespace contextual_tasks
