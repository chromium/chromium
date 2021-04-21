// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"

#include <utility>

#include "ash/components/audio/cras_audio_handler.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/user_image_source.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/arc/arc_prefs.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

bool IsPreferenceDefaultEnabled(const PrefService* prefs,
                                const std::string& path) {
  const PrefService::Preference* pref = prefs->FindPreference(path);

  if (pref->IsManaged())
    return pref->GetValue()->GetBool();

  if (pref->GetRecommendedValue())
    return pref->GetRecommendedValue()->GetBool();

  return true;
}

bool IsScreenContextDefaultEnabled(PrefService* prefs) {
  return IsPreferenceDefaultEnabled(
      prefs, chromeos::assistant::prefs::kAssistantContextEnabled);
}

bool IsScreenContextToggleDisabled(PrefService* prefs) {
  return prefs->IsManagedPreference(
      chromeos::assistant::prefs::kAssistantContextEnabled);
}

}  // namespace

namespace chromeos {

void RecordAssistantOptInStatus(AssistantOptInFlowStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.OptInFlowStatus", status, kMaxValue + 1);
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

// Construct SettingsUiUpdate for email opt-in.
assistant::SettingsUiUpdate GetEmailOptInUpdate(bool opted_in) {
  assistant::SettingsUiUpdate update;
  assistant::EmailOptInUpdate* email_optin_update =
      update.mutable_email_opt_in_update();
  email_optin_update->set_email_opt_in_update_state(
      opted_in ? assistant::EmailOptInUpdate::OPT_IN
               : assistant::EmailOptInUpdate::OPT_OUT);

  return update;
}

// Helper method to create zippy data.
base::Value CreateZippyData(const SettingZippyList& zippy_list) {
  base::Value zippy_data(base::Value::Type::LIST);
  for (auto& setting_zippy : zippy_list) {
    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("title", base::Value(setting_zippy.title()));
    if (setting_zippy.description_paragraph_size()) {
      data.SetKey("description",
                  base::Value(setting_zippy.description_paragraph(0)));
    }
    if (setting_zippy.additional_info_paragraph_size()) {
      data.SetKey("additionalInfo",
                  base::Value(setting_zippy.additional_info_paragraph(0)));
    }
    data.SetKey("iconUri", base::Value(setting_zippy.icon_uri()));
    data.SetKey("popupLink", base::Value(l10n_util::GetStringUTF16(
                                 IDS_ASSISTANT_ACTIVITY_CONTROL_POPUP_LINK)));
    zippy_data.Append(std::move(data));
  }
  return zippy_data;
}

// Helper method to create disclosure data.
base::Value CreateDisclosureData(const SettingZippyList& disclosure_list) {
  base::Value disclosure_data(base::Value::Type::LIST);
  for (auto& disclosure : disclosure_list) {
    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("title", base::Value(disclosure.title()));
    if (disclosure.description_paragraph_size()) {
      data.SetKey("description",
                  base::Value(disclosure.description_paragraph(0)));
    }
    if (disclosure.additional_info_paragraph_size()) {
      data.SetKey("additionalInfo",
                  base::Value(disclosure.additional_info_paragraph(0)));
    }
    data.SetKey("iconUri", base::Value(disclosure.icon_uri()));
    disclosure_data.Append(std::move(data));
  }
  return disclosure_data;
}

// Helper method to create get more screen data.
base::Value CreateGetMoreData(bool email_optin_needed,
                              const assistant::EmailOptInUi& email_optin_ui,
                              PrefService* prefs) {
  base::Value get_more_data(base::Value::Type::LIST);

  // Process screen context data.
  base::Value context_data(base::Value::Type::DICTIONARY);
  context_data.SetKey("id", base::Value("context"));
  context_data.SetKey("title", base::Value(l10n_util::GetStringUTF16(
                                   IDS_ASSISTANT_SCREEN_CONTEXT_TITLE)));
  context_data.SetKey("description", base::Value(l10n_util::GetStringUTF16(
                                         IDS_ASSISTANT_SCREEN_CONTEXT_DESC)));
  context_data.SetKey("defaultEnabled",
                      base::Value(IsScreenContextDefaultEnabled(prefs)));
  context_data.SetKey("toggleDisabled",
                      base::Value(IsScreenContextToggleDisabled(prefs)));
  context_data.SetKey(
      "iconUri",
      base::Value("https://www.gstatic.com/images/icons/material/system/"
                  "2x/screen_search_desktop_grey600_24dp.png"));
  get_more_data.Append(std::move(context_data));

  // Process email optin data.
  if (email_optin_needed) {
    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("id", base::Value("email"));
    data.SetKey("title", base::Value(email_optin_ui.title()));
    data.SetKey("description", base::Value(email_optin_ui.description()));
    data.SetKey("defaultEnabled",
                base::Value(email_optin_ui.default_enabled()));
    data.SetKey("iconUri", base::Value(email_optin_ui.icon_uri()));
    data.SetKey("legalText", base::Value(email_optin_ui.legal_text()));
    get_more_data.Append(std::move(data));
  }

  return get_more_data;
}

// Get string constants for settings ui.
base::Value GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
                                 bool activity_control_needed) {
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();
  auto third_party_disclosure_ui = consent_ui.third_party_disclosure_ui();
  base::Value dictionary(base::Value::Type::DICTIONARY);

  dictionary.SetKey("activityControlNeeded",
                    base::Value(activity_control_needed));

  // Add activity control string constants.
  if (activity_control_needed) {
    scoped_refptr<base::RefCountedMemory> image =
        chromeos::UserImageSource::GetUserImage(
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    std::string icon_url = webui::GetPngDataUrl(image->front(), image->size());
    dictionary.SetKey("valuePropUserImage", base::Value(icon_url));

    dictionary.SetKey("valuePropIdentity",
                      base::Value(activity_control_ui.identity()));
    dictionary.SetKey("valuePropTitle",
                      base::Value(activity_control_ui.title()));
    if (activity_control_ui.intro_text_paragraph_size()) {
      dictionary.SetKey(
          "valuePropIntro",
          base::Value(activity_control_ui.intro_text_paragraph(0)));
    }
    if (activity_control_ui.footer_paragraph_size()) {
      dictionary.SetKey("valuePropFooter",
                        base::Value(activity_control_ui.footer_paragraph(0)));
    }
    dictionary.SetKey("valuePropNextButton",
                      base::Value(consent_ui.accept_button_text()));
    dictionary.SetKey("valuePropSkipButton",
                      base::Value(consent_ui.reject_button_text()));
  }

  // Add third party string constants.
  dictionary.SetKey("thirdPartyTitle",
                    base::Value(third_party_disclosure_ui.title()));
  dictionary.SetKey("thirdPartyContinueButton",
                    base::Value(third_party_disclosure_ui.button_continue()));
  dictionary.SetKey("thirdPartyFooter", base::Value(consent_ui.tos_pp_links()));

  return dictionary;
}

void RecordActivityControlConsent(Profile* profile,
                                  std::string ui_audit_key,
                                  bool opted_in) {
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

  ConsentAuditorFactory::GetForProfile(profile)
      ->RecordAssistantActivityControlConsent(account_id, consent);
}

bool IsHotwordDspAvailable() {
  return chromeos::CrasAudioHandler::Get()->HasHotwordDevice();
}

bool IsVoiceMatchEnforcedOff(const PrefService* prefs) {
  // If the hotword preference is managed to always disabled, then we should not
  // show Voice Match flow.
  return prefs->IsManagedPreference(
             assistant::prefs::kAssistantHotwordEnabled) &&
         !prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled);
}

}  // namespace chromeos
