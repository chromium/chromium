// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/contextual_tasks/public/features.h"
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

  auto* vertical_tab_controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);
  if (vertical_tab_controller) {
    vertical_tab_subscription_ = vertical_tab_controller->RegisterOnModeChanged(
        base::BindRepeating(&ContextualTasksCloseButtonController::
                                OnVerticalTabStripModeChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  auto* immersive_controller =
      ImmersiveModeController::From(browser_window_interface_);
  if (immersive_controller) {
    immersive_mode_observation_.Observe(immersive_controller);
  }
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
  is_panel_visible_ = true;
  is_panel_hiding_ = false;
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    is_panel_hiding_ = true;
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksCloseButtonController::OnEntryHideCancelled(
    SidePanelEntry* entry) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    is_panel_hiding_ = false;
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksCloseButtonController::OnEntryHidden(
    SidePanelEntry* entry) {
  is_panel_hiding_ = false;
  is_panel_visible_ = false;
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::ExpandToFullTabStateChanged() {
  MaybeNotifyVisibilityShouldChange();
}

bool ContextualTasksCloseButtonController::IsVerticalTabOrIsImmersiveMode()
    const {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs)) {
    return false;
  }

  bool is_vertical_tabs = false;
  auto* vertical_tab_controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);
  if (vertical_tab_controller &&
      vertical_tab_controller->ShouldDisplayVerticalTabs()) {
    is_vertical_tabs = true;
  }

  bool is_immersive_mode = false;
  auto* immersive_controller =
      ImmersiveModeController::From(browser_window_interface_);
  if (immersive_controller && immersive_controller->IsEnabled()) {
    is_immersive_mode = true;
  }

  return is_vertical_tabs || is_immersive_mode;
}

void ContextualTasksCloseButtonController::OnVerticalTabStripModeChanged(
    tabs::VerticalTabStripStateController* controller) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::OnImmersiveFullscreenEntered() {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksCloseButtonController::OnImmersiveFullscreenExited() {
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
  const bool can_expand_to_full_tab =
      controller && controller->CanExpandToFullTab();

  /** The close tab button is only visible when:
   *  - Browser is not in vertical tab mode.
   *  - Browser is not in immersive mode.
   *  - Entry point is eligible.
   *  - Side panel is open.
   *  - Side panel can expand to full tab.
   **/
  bool is_panel_state_eligible = is_panel_visible_ && !is_panel_hiding_;
  should_update_visibility_callbacks_.Notify(
      !IsVerticalTabOrIsImmersiveMode() && is_eligible &&
      is_panel_state_eligible && can_expand_to_full_tab);

  if (controller) {
    content::WebContents* contents = controller->GetActiveWebContents();
    if (contents) {
      auto* ui = contextual_tasks::GetWebUiInterface(contents);
      if (ui) {
        // The expand button option is enabled if the experimental flag is set,
        // provided that the WebUI is eligible to expand to a full tab (checking
        // co-browse eligibility strictly from its static load-time cached
        // state).
        bool enabled =
            (IsVerticalTabOrIsImmersiveMode() ||
             (contextual_tasks::GetExpandButtonOption() ==
              contextual_tasks::ExpandButtonOption::kSidePanelExpandButton)) &&
            ui->CanExpandToFullTab();

        ui->UpdateExpandButtonEnabled(enabled);
      }
    }
  }
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
  auto* tab_strip_model = browser_window_interface_->GetTabStripModel();
  CHECK(tab_strip_model);
  tab_strip_model->CloseWebContentsAt(
      tab_strip_model->GetIndexOfTab(active_tab),
      TabCloseTypes::CLOSE_EXPAND_SIDE_PANEL);
}
