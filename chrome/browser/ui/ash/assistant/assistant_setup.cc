// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_setup.h"

#include <string>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using chromeos::assistant::ConsentFlowUi;

namespace {

PrefService* Prefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

}  // namespace

AssistantSetup::AssistantSetup() {
  DCHECK(assistant::IsAssistantAllowedForProfile(
             ProfileManager::GetActiveUserProfile()) ==
         chromeos::assistant::AssistantAllowedState::ALLOWED);
  ash::AssistantState::Get()->AddObserver(this);

  search_and_assistant_enabled_checker_ =
      std::make_unique<SearchAndAssistantEnabledChecker>(
          ProfileManager::GetActiveUserProfile()->GetURLLoaderFactory().get(),
          this);
  search_and_assistant_enabled_checker_->SyncSearchAndAssistantState();
}

AssistantSetup::~AssistantSetup() {
  ash::AssistantState::Get()->RemoveObserver(this);
}

void AssistantSetup::StartAssistantOptInFlow(
    ash::FlowType type,
    StartAssistantOptInFlowCallback callback) {
  chromeos::AssistantOptInDialog::Show(type, std::move(callback));
}

bool AssistantSetup::BounceOptInWindowIfActive() {
  return chromeos::AssistantOptInDialog::BounceIfActive();
}

void AssistantSetup::MaybeStartAssistantOptInFlow() {
  DCHECK(Prefs());
  if (!Prefs()->GetUserPrefValue(
          chromeos::assistant::prefs::kAssistantConsentStatus)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantSetup::StartAssistantOptInFlow,
                       weak_factory_.GetWeakPtr(), ash::FlowType::kConsentFlow,
                       base::DoNothing::Once<bool>()));
  }
}

void AssistantSetup::OnError() {
  // When there is an error, default the pref to false if the pref is not set
  // yet. The pref value will be synced the next time.
  if (!Prefs()->GetUserPrefValue(
          chromeos::assistant::prefs::kAssistantDisabledByPolicy)) {
    Prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantDisabledByPolicy,
                        false);
  }
}

void AssistantSetup::OnSearchAndAssistantStateReceived(bool is_disabled) {
  Prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantDisabledByPolicy,
                      is_disabled);
  if (is_disabled) {
    DVLOG(1) << "Assistant is disabled by domain policy.";
    Prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  }
}

void AssistantSetup::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  if (status == chromeos::assistant::AssistantStatus::NOT_READY)
    return;

  SyncSettingsState();
}

void AssistantSetup::SyncSettingsState() {
  chromeos::assistant::SettingsUiSelector selector;
  chromeos::assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(
      chromeos::assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  chromeos::assistant::AssistantSettings::Get()->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantSetup::OnGetSettingsResponse,
                     base::Unretained(this)));
}

void AssistantSetup::OnGetSettingsResponse(const std::string& settings) {
  chromeos::assistant::SettingsUi settings_ui;
  if (!settings_ui.ParseFromString(settings))
    return;

  // Sync activity control status.
  if (!settings_ui.has_consent_flow_ui()) {
    LOG(ERROR) << "Failed to get activity control status.";
    return;
  }
  const auto& consent_status = settings_ui.consent_flow_ui().consent_status();
  const auto& consent_ui = settings_ui.consent_flow_ui().consent_ui();
  switch (consent_status) {
    case ConsentFlowUi::ASK_FOR_CONSENT:
      if (consent_ui.has_activity_control_ui() &&
          consent_ui.activity_control_ui().setting_zippy().size()) {
        Prefs()->SetInteger(
            chromeos::assistant::prefs::kAssistantConsentStatus,
            chromeos::assistant::prefs::ConsentStatus::kNotFound);
      } else {
        Prefs()->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                            chromeos::assistant::prefs::ConsentStatus::
                                kActivityControlAccepted);
      }
      break;
    case ConsentFlowUi::ERROR_ACCOUNT:
      Prefs()->SetInteger(
          chromeos::assistant::prefs::kAssistantConsentStatus,
          chromeos::assistant::prefs::ConsentStatus::kUnauthorized);
      break;
    case ConsentFlowUi::ALREADY_CONSENTED:
      Prefs()->SetInteger(
          chromeos::assistant::prefs::kAssistantConsentStatus,
          chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);
      break;
    case ConsentFlowUi::UNSPECIFIED:
    case ConsentFlowUi::ERROR:
      Prefs()->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                          chromeos::assistant::prefs::ConsentStatus::kUnknown);
      LOG(ERROR) << "Invalid activity control consent status.";
  }
}
