// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_setup.h"

#include <string>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/prefs/pref_service.h"

using chromeos::assistant::ConsentFlowUi;

AssistantSetup::AssistantSetup(
    chromeos::assistant::mojom::AssistantService* service)
    : service_(service) {
  ash::AssistantState::Get()->AddObserver(this);
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

void AssistantSetup::OnAssistantStatusChanged(
    ash::mojom::AssistantState state) {
  if (state == ash::mojom::AssistantState::NOT_READY)
    return;

  // Sync settings state when Assistant service started.
  if (!settings_manager_)
    SyncSettingsState();
}

void AssistantSetup::SyncSettingsState() {
  // Set up settings mojom.
  service_->BindSettingsManager(settings_manager_.BindNewPipeAndPassReceiver());

  chromeos::assistant::SettingsUiSelector selector;
  chromeos::assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(
      chromeos::assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_gaia_user_context_ui(true);
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantSetup::OnGetSettingsResponse,
                     base::Unretained(this)));
}

void AssistantSetup::OnGetSettingsResponse(const std::string& settings) {
  chromeos::assistant::SettingsUi settings_ui;
  if (!settings_ui.ParseFromString(settings))
    return;

  // Sync domain policy status.
  if (settings_ui.has_gaia_user_context_ui()) {
    const auto& gaia_user_context_ui = settings_ui.gaia_user_context_ui();
    if (gaia_user_context_ui.assistant_disabled_by_dasher_domain()) {
      DVLOG(1) << "Assistant is disabled by domain policy.";
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetBoolean(chromeos::assistant::prefs::kAssistantDisabledByPolicy,
                        true);
      prefs->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
      return;
    }
  } else {
    LOG(ERROR) << "Failed to get gaia user context";
  }

  // Sync activity control status.
  if (!settings_ui.has_consent_flow_ui()) {
    LOG(ERROR) << "Failed to get activity control status.";
    return;
  }
  const auto& consent_status = settings_ui.consent_flow_ui().consent_status();
  const auto& consent_ui = settings_ui.consent_flow_ui().consent_ui();
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* prefs = profile->GetPrefs();
  switch (consent_status) {
    case ConsentFlowUi::ASK_FOR_CONSENT:
      if (consent_ui.has_activity_control_ui() &&
          consent_ui.activity_control_ui().setting_zippy().size()) {
        prefs->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                          chromeos::assistant::prefs::ConsentStatus::kNotFound);
      } else {
        prefs->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                          chromeos::assistant::prefs::ConsentStatus::
                              kActivityControlAccepted);
      }
      break;
    case ConsentFlowUi::ERROR_ACCOUNT:
      prefs->SetInteger(
          chromeos::assistant::prefs::kAssistantConsentStatus,
          chromeos::assistant::prefs::ConsentStatus::kUnauthorized);
      break;
    case ConsentFlowUi::ALREADY_CONSENTED:
      prefs->SetInteger(
          chromeos::assistant::prefs::kAssistantConsentStatus,
          chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted);
      break;
    case ConsentFlowUi::UNSPECIFIED:
    case ConsentFlowUi::ERROR:
      prefs->SetInteger(chromeos::assistant::prefs::kAssistantConsentStatus,
                        chromeos::assistant::prefs::ConsentStatus::kUnknown);
      LOG(ERROR) << "Invalid activity control consent status.";
  }
}

void AssistantSetup::MaybeStartAssistantOptInFlow() {
  auto* pref_service = ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(pref_service);
  if (!pref_service->GetUserPrefValue(
          chromeos::assistant::prefs::kAssistantConsentStatus)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantSetup::StartAssistantOptInFlow,
                       weak_factory_.GetWeakPtr(), ash::FlowType::kConsentFlow,
                       base::DoNothing::Once<bool>()));
  }
}
