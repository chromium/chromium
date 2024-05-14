// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_utils.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/proto/activity_control_settings_common.pb.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

using AssistantActivityControlConsent =
    sync_pb::UserConsentTypes::AssistantActivityControlConsent;

namespace ash {

namespace {

// Possible native assistant icons
// Must be in sync with the corresponding javascript enum.
enum class AssistantNativeIconType {
  kNone = 0,

  // Web & App Activity.
  kWAA = 1,

  // Device Applications Information.
  kDA = 2,

  kInfo = 3,
};

AssistantNativeIconType SettingIdToIconType(
    assistant::SettingSetId setting_set_id) {
  switch (setting_set_id) {
    case assistant::SettingSetId::WAA:
      return AssistantNativeIconType::kWAA;
    case assistant::SettingSetId::DA:
      return AssistantNativeIconType::kDA;
    case assistant::SettingSetId::UNKNOWN_SETTING_SET_ID:
      NOTREACHED_IN_MIGRATION();
      return AssistantNativeIconType::kNone;
  }
}

}  // namespace

void RecordAssistantOptInStatus(AssistantOptInFlowStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Assistant.OptInFlowStatus", status,
      static_cast<int>(AssistantOptInFlowStatus::kMaxValue) + 1);
}

void RecordAssistantActivityControlOptInStatus(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type,
    bool opted_in) {
  AssistantOptInFlowStatus status;
  switch (setting_type) {
    case AssistantActivityControlConsent::ALL:
    case AssistantActivityControlConsent::SETTING_TYPE_UNSPECIFIED:
      status = opted_in ? AssistantOptInFlowStatus::kActivityControlAccepted
                        : AssistantOptInFlowStatus::kActivityControlSkipped;
      break;
    case AssistantActivityControlConsent::WEB_AND_APP_ACTIVITY:
      status = opted_in ? AssistantOptInFlowStatus::kActivityControlWaaAccepted
                        : AssistantOptInFlowStatus::kActivityControlWaaSkipped;
      break;
    case AssistantActivityControlConsent::DEVICE_APPS:
      status = opted_in ? AssistantOptInFlowStatus::kActivityControlDaAccepted
                        : AssistantOptInFlowStatus::kActivityControlDaSkipped;
      break;
  }
  RecordAssistantOptInStatus(status);
}

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector() {
  assistant::SettingsUiSelector selector;
  assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(assistant::ActivityControlSettingsUiSelector::
                                   ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_email_opt_in(true);
  selector.set_gaia_user_context_ui(true);
  return selector;
}

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token) {
  assistant::SettingsUiUpdate update;
  assistant::ConsentFlowUiUpdate* consent_flow_update =
      update.mutable_consent_flow_ui_update();
  consent_flow_update->set_flow_id(
      assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  consent_flow_update->set_consent_token(consent_token);

  return update;
}

// Helper method to create zippy data.
base::Value::List CreateZippyData(const ActivityControlUi& activity_control_ui,
                                  bool is_minor_mode) {
  base::Value::List zippy_data;
  auto zippy_list = activity_control_ui.setting_zippy();
  auto learn_more_dialog = activity_control_ui.learn_more_dialog();
  for (auto& setting_zippy : zippy_list) {
    auto data =
        base::Value::Dict()
            .Set("title", activity_control_ui.title())
            .Set("identity", activity_control_ui.identity())
            .Set("name", setting_zippy.title())
            .Set("iconUri", setting_zippy.icon_uri())
            .Set("nativeIconType", static_cast<int>(SettingIdToIconType(
                                       setting_zippy.setting_set_id())))
            .Set("useNativeIcons", features::IsAssistantNativeIconsEnabled())
            .Set("popupLink", l10n_util::GetStringUTF16(
                                  IDS_ASSISTANT_ACTIVITY_CONTROL_POPUP_LINK))
            .Set("learnMoreDialogButton", learn_more_dialog.dismiss_button())
            .Set("isMinorMode", is_minor_mode);
    if (activity_control_ui.intro_text_paragraph_size()) {
      data.Set("intro", activity_control_ui.intro_text_paragraph(0));
    }
    if (setting_zippy.description_paragraph_size()) {
      data.Set("description", setting_zippy.description_paragraph(0));
    }
    if (setting_zippy.additional_info_paragraph_size()) {
      data.Set("additionalInfo", setting_zippy.additional_info_paragraph(0));
    }
    if (is_minor_mode) {
      data.Set("learnMoreDialogTitle", learn_more_dialog.title());
      if (learn_more_dialog.paragraph_size()) {
        data.Set("learnMoreDialogContent",
                 learn_more_dialog.paragraph(0).value());
      }
    } else {
      data.Set("learnMoreDialogTitle", setting_zippy.title());
      if (setting_zippy.additional_info_paragraph_size()) {
        data.Set("learnMoreDialogContent",
                 setting_zippy.additional_info_paragraph(0));
      }
    }
    zippy_data.Append(std::move(data));
  }
  return zippy_data;
}

// Helper method to create disclosure data.
base::Value::List CreateDisclosureData(
    const SettingZippyList& disclosure_list) {
  base::Value::List disclosure_data;
  for (auto& disclosure : disclosure_list) {
    auto data = base::Value::Dict()
                    .Set("title", disclosure.title())
                    .Set("iconUri", disclosure.icon_uri());
    if (disclosure.description_paragraph_size()) {
      data.Set("description", disclosure.description_paragraph(0));
    }
    if (disclosure.additional_info_paragraph_size()) {
      data.Set("additionalInfo", disclosure.additional_info_paragraph(0));
    }
    disclosure_data.Append(std::move(data));
  }
  return disclosure_data;
}

// Get string constants for settings ui.
base::Value::Dict GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
                                       bool activity_control_needed,
                                       bool equal_weight_buttons) {
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();

  auto dictionary = base::Value::Dict()
                        .Set("activityControlNeeded", activity_control_needed)
                        .Set("equalWeightButtons", equal_weight_buttons);

  // Add activity control string constants.
  if (activity_control_needed) {
    dictionary.Set("valuePropTitle", activity_control_ui.title());
    if (activity_control_ui.footer_paragraph_size()) {
      dictionary.Set("valuePropFooter",
                     activity_control_ui.footer_paragraph(0));
    }
    dictionary.Set("valuePropNextButton", consent_ui.accept_button_text());
    dictionary.Set("valuePropSkipButton", consent_ui.reject_button_text());
  }

  return dictionary;
}

void RecordActivityControlConsent(
    Profile* profile,
    std::string ui_audit_key,
    bool opted_in,
    AssistantActivityControlConsent::SettingType setting_type) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // This function doesn't care about browser sync consent.
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  using sync_pb::UserConsentTypes;
  UserConsentTypes::AssistantActivityControlConsent consent;
  consent.set_ui_audit_key(ui_audit_key);
  consent.set_status(opted_in ? UserConsentTypes::GIVEN
                              : UserConsentTypes::NOT_GIVEN);
  consent.set_setting_type(setting_type);

  ConsentAuditorFactory::GetForProfile(profile)
      ->RecordAssistantActivityControlConsent(account_id, consent);
}

bool IsHotwordDspAvailable() {
  return CrasAudioHandler::Get()->HasHotwordDevice();
}

bool IsVoiceMatchEnforcedOff(const PrefService* prefs,
                             bool is_oobe_in_progress) {
  // If the hotword preference is managed to always disabled Voice Match flow is
  // hidden.
  if (prefs->IsManagedPreference(assistant::prefs::kAssistantHotwordEnabled) &&
      !prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled)) {
    return true;
  }
  // If Voice Match is disabled by policy during OOBE, then Voice Match flow is
  // hidden.
  if (is_oobe_in_progress &&
      !prefs->GetBoolean(
          assistant::prefs::kAssistantVoiceMatchEnabledDuringOobe)) {
    return true;
  }
  return false;
}

AssistantActivityControlConsent::SettingType
GetActivityControlConsentSettingType(const SettingZippyList& setting_zippy) {
  if (setting_zippy.size() > 1) {
    return AssistantActivityControlConsent::ALL;
  }
  auto setting_id = setting_zippy[0].setting_set_id();
  if (setting_id == assistant::SettingSetId::DA) {
    return AssistantActivityControlConsent::DEVICE_APPS;
  }
  if (setting_id == assistant::SettingSetId::WAA) {
    return AssistantActivityControlConsent::WEB_AND_APP_ACTIVITY;
  }
  return AssistantActivityControlConsent::SETTING_TYPE_UNSPECIFIED;
}

}  // namespace ash
