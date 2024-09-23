// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/gaia_action_buttons_resources.h"
#include "chrome/grit/gaia_action_buttons_resources_map.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lock_screen_reauth_resources.h"
#include "chrome/grit/lock_screen_reauth_resources_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

bool LockScreenStartReauthUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::ProfileHelper::IsLockScreenProfile(
      Profile::FromBrowserContext(browser_context));
}

LockScreenStartReauthUI::LockScreenStartReauthUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  std::string email;
  if (user) {
    email = user->GetDisplayEmail();
  }

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUILockScreenStartReauthHost);
  ash::EnableTrustedTypesCSP(source);

  auto main_handler = std::make_unique<LockScreenReauthHandler>(email);
  main_handler_ = main_handler.get();
  web_ui->AddMessageHandler(std::move(main_handler));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  source->EnableReplaceI18nInJS();
  source->UseStringsJs();

  source->AddString(
      "lockScreenReauthSubtitile1WithError",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_WRONG_USER_SUBTITLE1));
  source->AddString(
      "lockScreenReauthSubtitile2WithError",
      l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_WRONG_USER_SUBTITLE2,
                                 base::UTF8ToUTF16(email)));
  source->AddString("lockScreenVerifyButton",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_BUTTON));
  source->AddString(
      "lockScreenVerifyAgainButton",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_AGAIN_BUTTON));
  source->AddString("lockScreenCancelButton",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CANCEL_BUTTON));
  source->AddString("lockScreenCloseButton",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CLOSE_BUTTON));
  source->AddString(
      "lockScreenNextButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_SAML_INTERSTITIAL_NEXT_BUTTON_TEXT));
  source->AddString(
      "confirmPasswordLabel",
      l10n_util::GetStringUTF16(IDS_LOGIN_CONFIRM_PASSWORD_LABEL));
  source->AddString(
      "manualPasswordInputLabel",
      l10n_util::GetStringUTF16(IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL));
  source->AddString("passwordChangedIncorrectOldPassword",
                    l10n_util::GetStringUTF16(
                        IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD));
  source->AddString(
      "manualPasswordMismatch",
      l10n_util::GetStringUTF16(IDS_LOGIN_MANUAL_PASSWORD_MISMATCH));
  source->AddString("loginWelcomeMessage",
                    l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFY_ACCOUNT));
  source->AddString(
      "loginWelcomeMessageWithError",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_VERIFICATION_FAILED));
  source->AddString(
      "manualPasswordSubtitle",
      l10n_util::GetStringUTF16(IDS_LOCK_MANUAL_PASSWORD_SUBTITLE));
  source->AddString("confirmPasswordSubtitle",
                    l10n_util::GetStringFUTF16(IDS_LOGIN_CONFIRM_PASSWORD_TITLE,
                                               ui::GetChromeOSDeviceName()));
  source->AddString("samlNotice",
                    l10n_util::GetStringUTF16(IDS_LOCK_SAML_NOTICE));
  source->AddString("passwordChangedTitle",
                    l10n_util::GetStringUTF16(IDS_LOCK_PASSWORD_CHANGED_TITLE));
  source->AddString(
      "passwordChangedSubtitle",
      l10n_util::GetStringFUTF16(IDS_LOCK_PASSWORD_CHANGED_SUBTITLE,
                                 ui::GetChromeOSDeviceName()));
  source->AddString(
      "passwordChangedOldPasswordHint",
      l10n_util::GetStringUTF16(IDS_LOCK_PASSWORD_CHANGED_OLD_PASSWORD_HINT));

  source->AddString(
      "samlChangeProviderMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_SAML_CHANGE_PROVIDER_MESSAGE));
  source->AddString(
      "samlChangeProviderButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_SAML_CHANGE_PROVIDER_BUTTON));
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  bool policy_ca_certs_present = primary_profile
                                     ? primary_profile->GetPrefs()->GetBoolean(
                                           prefs::kUsedPolicyCertificates)
                                     : false;
  source->AddBoolean("policyProvidedCaCertsPresent", policy_ca_certs_present);
  source->AddString(
      "policyProvidedCaCertsTooltipMessage",
      l10n_util::GetStringUTF16(
          IDS_CUSTOM_POLICY_PROVIDED_TRUST_ANCHORS_AT_LOCK_SCREEN_TOOLTIP));

  source->AddResourcePaths(base::make_span(kLockScreenReauthResources,
                                           kLockScreenReauthResourcesSize));
  source->AddResourcePaths(base::make_span(kGaiaActionButtonsResources,
                                           kGaiaActionButtonsResourcesSize));
  source->SetDefaultResource(
      IDR_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_APP_HTML);

  // Add OOBE and Gaia Authenticator resources
  OobeUI::AddOobeComponents(source);
}

LockScreenStartReauthUI::~LockScreenStartReauthUI() = default;

}  // namespace ash
