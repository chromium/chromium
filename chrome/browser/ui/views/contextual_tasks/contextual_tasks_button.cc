// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksButton,
                                      kContextualTasksToolbarButton);

ContextualTasksButton::ContextualTasksButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(base::BindRepeating(&ContextualTasksButton::OnButtonPress,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_window_interface_(browser_window_interface) {
  SetVectorIcon(kDockToRightSparkIcon);
  SetProperty(views::kElementIdentifierKey, kContextualTasksToolbarButton);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TITLE));

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarPermanent) {
    pin_state_.Init(
        prefs::kPinContextualTaskButton,
        browser_window_interface->GetProfile()->GetPrefs(),
        base::BindRepeating(&ContextualTasksButton::OnPinStateChanged,
                            base::Unretained(this)));
  } else {
    CHECK_EQ(contextual_tasks::kShowEntryPoint.Get(),
             contextual_tasks::EntryPointOption::kToolbarRevisit);
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    should_update_visibility_subscription_ =
        controller->RegisterShouldUpdateButtonVisibility(base::BindRepeating(
            &ContextualTasksButton::OnShouldUpdateVisibility,
            base::Unretained(this)));
  }

  eligibility_change_subscription_ =
      contextual_tasks::EntryPointEligibilityManager::From(
          browser_window_interface_)
          ->RegisterOnEntryPointEligibilityChanged(
              base::BindRepeating(&ContextualTasksButton::OnEligibilityChange,
                                  base::Unretained(this)));
  MaybeUpdateVisibility();
}

ContextualTasksButton::~ContextualTasksButton() = default;

void ContextualTasksButton::OnButtonPress() {
  const auto* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(
          browser_window_interface_);
  CHECK(coordinator);
  if (coordinator->IsSidePanelOpenForContextualTask()) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel", true);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel", true);
  }

  actions::ActionManager::Get()
      .FindAction(kActionSidePanelShowContextualTasks,
                  browser_window_interface_->GetActions()->root_action_item())
      ->InvokeAction();
}

void ContextualTasksButton::OnPinStateChanged() {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnShouldUpdateVisibility(bool should_show) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnEligibilityChange(bool is_eligible) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::MaybeUpdateVisibility() {
  const bool is_button_eligible =
      contextual_tasks::EntryPointEligibilityManager::From(
          browser_window_interface_)
          ->AreEntryPointsEligible();
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarPermanent) {
    SetVisible(is_button_eligible && pin_state_.GetValue());
  } else if (contextual_tasks::kShowEntryPoint.Get() ==
             contextual_tasks::EntryPointOption::kToolbarRevisit) {
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    SetVisible(is_button_eligible && controller->ShouldShowEphemeralButton());
  }
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
