// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BROWSER_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"

class ContextualTasksEphemeralButtonController;
class ContextualTasksCloseButtonController;
#endif

namespace contextual_tasks {

// Manages the browser-window-scoped lifecycle, initialization, and eligibility
// callbacks for all Contextual Tasks entry point and side panel controllers.
// Accessible from BrowserWindowInterface via the UnownedUserData pattern.
class ContextualTasksBrowserController
#if !BUILDFLAG(IS_ANDROID)
    : public PinnedToolbarActionsModel::Observer
#endif
{
 public:
  DECLARE_USER_DATA(ContextualTasksBrowserController);

  explicit ContextualTasksBrowserController(
      BrowserWindowInterface* browser_window_interface);
  ~ContextualTasksBrowserController() override;

  static ContextualTasksBrowserController* From(
      BrowserWindowInterface* browser_window_interface);

  void Shutdown();

  EntryPointEligibilityManager* eligibility_manager() {
    return eligibility_manager_.get();
  }

  ContextualTasksSidePanelCoordinator* side_panel_coordinator() {
    return side_panel_coordinator_.get();
  }

#if !BUILDFLAG(IS_ANDROID)
  // PinnedToolbarActionsModel::Observer:
  void OnActionsChanged() override;

  ContextualTasksEphemeralButtonController* ephemeral_button_controller() {
    return ephemeral_button_controller_.get();
  }

  ContextualTasksCloseButtonController* close_button_controller() {
    return close_button_controller_.get();
  }
#endif

 private:
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<ContextualTasksBrowserController>
      scoped_unowned_user_data_;

  std::unique_ptr<ActiveTaskContextProvider> active_task_context_provider_;
  std::unique_ptr<EntryPointEligibilityManager> eligibility_manager_;
  std::unique_ptr<ContextualTasksSidePanelCoordinator> side_panel_coordinator_;

#if !BUILDFLAG(IS_ANDROID)
  void OnEntryPointEligibilityChanged(bool is_eligible);
  void MaybeShowPinnedIconChangedPromo();

  std::unique_ptr<ContextualTasksEphemeralButtonController>
      ephemeral_button_controller_;
  std::unique_ptr<ContextualTasksCloseButtonController>
      close_button_controller_;
  base::CallbackListSubscription actions_visibility_subscription_;
  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_toolbar_observation_{this};
  bool was_pinned_ = false;
  bool has_requested_pinned_icon_changed_promo_ = false;
#endif
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BROWSER_CONTROLLER_H_
