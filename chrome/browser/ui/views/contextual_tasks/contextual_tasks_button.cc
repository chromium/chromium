// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
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
    OnPinStateChanged();
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
    // The button should not be visible until the active tab is associated with
    // a task.
    SetVisible(false);
  }
}

ContextualTasksButton::~ContextualTasksButton() = default;

void ContextualTasksButton::OnButtonPress() {
  actions::ActionManager::Get()
      .FindAction(kActionSidePanelShowContextualTasks,
                  browser_window_interface_->GetActions()->root_action_item())
      ->InvokeAction();
}

void ContextualTasksButton::OnPinStateChanged() {
  SetVisible(pin_state_.GetValue());
}

void ContextualTasksButton::OnShouldUpdateVisibility(bool should_show) {
  SetVisible(should_show);
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
