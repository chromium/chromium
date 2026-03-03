// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(ContextualTasksCloseButtonController);

ContextualTasksCloseButtonController::ContextualTasksCloseButtonController(
    BrowserWindowInterface* browser_window_interface,
    contextual_tasks::EntryPointEligibilityManager* eligibility_manager,
    contextual_tasks::ContextualTasksPanelController* panel_controller)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  auto* registry = SidePanelRegistry::From(browser_window_interface_);
  if (registry) {
    auto* entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
    if (entry) {
      side_panel_entry_observation_.Observe(entry);
    }
  }

  eligibility_change_subscription_ =
      eligibility_manager->RegisterOnEntryPointEligibilityChanged(
          base::BindRepeating(
              &ContextualTasksCloseButtonController::OnEligibilityChange,
              weak_ptr_factory_.GetWeakPtr()));
  panel_controller_observation_.Observe(panel_controller);
}

ContextualTasksCloseButtonController::~ContextualTasksCloseButtonController() =
    default;

// static:
ContextualTasksCloseButtonController*
ContextualTasksCloseButtonController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void ContextualTasksCloseButtonController::OnEntryShown(SidePanelEntry* entry) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::OnEntryHidden(
    SidePanelEntry* entry) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::ExpandToFullTabStateChanged() {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::OnEligibilityChange(
    bool is_eligible) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::MaybeNotifyVisibilityShouldChange() {
  auto* eligibility_manager =
      contextual_tasks::EntryPointEligibilityManager::From(
          browser_window_interface_);
  const bool is_eligible =
      eligibility_manager && eligibility_manager->AreEntryPointsEligible();
  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser_window_interface_);
  const bool is_contextual_tasks_panel_open =
      controller && controller->IsPanelOpenForContextualTask();
  const bool can_expand_to_full_tab =
      controller && controller->CanExpandToFullTab();

  /** The close tab button is only visible when:
   *  - Entry point is eligible.
   *  - Side panel is open.
   *  - Side panel can expand to full tab.
   **/
  should_update_visibility_callbacks_.Notify(
      is_eligible && is_contextual_tasks_panel_open && can_expand_to_full_tab);
}

base::CallbackListSubscription
ContextualTasksCloseButtonController::RegisterShouldUpdateButtonVisibility(
    ShouldUpdateVisibilityCallbackList::CallbackType callback) {
  return should_update_visibility_callbacks_.Add(std::move(callback));
}

void ContextualTasksCloseButtonController::MaybeCloseTabExpandSidePanel() {
  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser_window_interface_);
  CHECK(controller);

  if (!controller->IsPanelOpenForContextualTask()) {
    return;
  }

  tabs::TabInterface* active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }

  // TODO(b/487328229): Call MoveTaskUiToNewTab after the active tab is
  // closed since beforeunload event handler can intercept the close tab
  // action.
  controller->MoveTaskUiToNewTab();
  // TODO(b/487328229): Disable the close tab animation in this case.
  active_tab->Close();
}
