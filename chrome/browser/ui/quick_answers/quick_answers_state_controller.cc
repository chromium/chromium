// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_controller.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/prefs/pref_service.h"

namespace {

using chromeos::assistant::prefs::kAssistantContextEnabled;
using chromeos::assistant::prefs::kAssistantEnabled;
using quick_answers::prefs::ConsentStatus;
using quick_answers::prefs::kQuickAnswersConsentStatus;
using quick_answers::prefs::kQuickAnswersEnabled;
using quick_answers::prefs::kQuickAnswersNoticeImpressionCount;

void MigrateQuickAnswersConsentStatus(PrefService* prefs) {
  // If the consented status has not been set, migrate the current context
  // enabled value.
  if (prefs->FindPreference(kQuickAnswersConsentStatus)->IsDefaultValue()) {
    if (!prefs->FindPreference(kAssistantContextEnabled)->IsDefaultValue()) {
      // Set the consent status based on current feature eligibility.
      bool consented =
          ash::AssistantState::Get()->allowed_state() ==
              chromeos::assistant::AssistantAllowedState::ALLOWED &&
          prefs->GetBoolean(kAssistantEnabled) &&
          prefs->GetBoolean(kAssistantContextEnabled);
      prefs->SetInteger(
          kQuickAnswersConsentStatus,
          consented ? ConsentStatus::kAccepted : ConsentStatus::kRejected);
      // Enable Quick Answers settings for existing users.
      if (consented)
        prefs->SetBoolean(kQuickAnswersEnabled, true);
    } else {
      // Set the consent status to unknown for new users.
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kUnknown);
      // Reset the impression count for new users.
      prefs->SetInteger(kQuickAnswersNoticeImpressionCount, 0);
    }
  }
}

}  // namespace

QuickAnswersStateController::QuickAnswersStateController()
    : session_observer_(this) {}

QuickAnswersStateController::~QuickAnswersStateController() = default;

void QuickAnswersStateController::OnFirstSessionStarted() {
  PrefService* prefs =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  state_.RegisterPrefChanges(prefs);
  MigrateQuickAnswersConsentStatus(prefs);
}
