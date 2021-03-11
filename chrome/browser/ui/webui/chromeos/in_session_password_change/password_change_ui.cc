// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/password_expiry_notification.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/urgent_password_expiry_notification_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

std::string GetPasswordChangeUrl(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSamlPasswordChangeUrl)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kSamlPasswordChangeUrl);
  }

  const policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (user_cloud_policy_manager) {
    const enterprise_management::PolicyData* policy =
        user_cloud_policy_manager->core()->store()->policy();
    if (policy->has_change_password_uri()) {
      return policy->change_password_uri();
    }
  }

  return SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs())
      .password_change_url();
}

std::u16string GetHostedHeaderText(const std::string& password_change_url) {
  std::u16string host =
      base::UTF8ToUTF16(net::GetHostAndOptionalPort(GURL(password_change_url)));
  DCHECK(!host.empty());
  return l10n_util::GetStringFUTF16(IDS_LOGIN_SAML_PASSWORD_CHANGE_NOTICE,
                                    host);
}

void AddSize(content::WebUIDataSource* source,
             const std::string& suffix,
             const gfx::Size& size) {
  source->AddInteger("width" + suffix, size.width());
  source->AddInteger("height" + suffix, size.height());
}

}  // namespace

PasswordChangeUI::PasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordChangeHost);

  const std::string password_change_url = GetPasswordChangeUrl(profile);
  web_ui->AddMessageHandler(
      std::make_unique<PasswordChangeHandler>(password_change_url));

  source->DisableTrustedTypesCSP();

  source->AddString("hostedHeader", GetHostedHeaderText(password_change_url));
  source->UseStringsJs();

  source->SetDefaultResource(IDR_PASSWORD_CHANGE_HTML);

  source->AddResourcePath("authenticator.js",
                          IDR_PASSWORD_CHANGE_AUTHENTICATOR_JS);
  source->AddResourcePath("webview_saml_injected.js",
                          IDR_GAIA_AUTH_WEBVIEW_SAML_INJECTED_JS);
  source->AddResourcePath("password_change.js", IDR_PASSWORD_CHANGE_JS);

  content::WebUIDataSource::Add(profile, source);
}

PasswordChangeUI::~PasswordChangeUI() = default;

ConfirmPasswordChangeUI::ConfirmPasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIConfirmPasswordChangeHost);

  source->DisableTrustedTypesCSP();

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_TITLE},
      {"bothPasswordsPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_BOTH_PASSWORDS_PROMPT},
      {"oldPasswordPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_OLD_PASSWORD_PROMPT},
      {"newPasswordPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_NEW_PASSWORD_PROMPT},
      {"oldPassword", IDS_PASSWORD_CHANGE_OLD_PASSWORD_LABEL},
      {"newPassword", IDS_PASSWORD_CHANGE_NEW_PASSWORD_LABEL},
      {"confirmNewPassword", IDS_PASSWORD_CHANGE_CONFIRM_NEW_PASSWORD_LABEL},
      {"incorrectPassword", IDS_LOGIN_CONFIRM_PASSWORD_INCORRECT_PASSWORD},
      {"matchError", IDS_PASSWORD_CHANGE_PASSWORDS_DONT_MATCH},
      {"save", IDS_PASSWORD_CHANGE_CONFIRM_SAVE_BUTTON}};

  source->AddLocalizedStrings(kLocalizedStrings);

  AddSize(source, "", ConfirmPasswordChangeDialog::GetSize(false, false));
  AddSize(source, "Old", ConfirmPasswordChangeDialog::GetSize(true, false));
  AddSize(source, "New", ConfirmPasswordChangeDialog::GetSize(false, true));
  AddSize(source, "OldNew", ConfirmPasswordChangeDialog::GetSize(true, true));

  source->UseStringsJs();
  source->SetDefaultResource(IDR_CONFIRM_PASSWORD_CHANGE_HTML);
  source->AddResourcePath("confirm_password_change.js",
                          IDR_CONFIRM_PASSWORD_CHANGE_JS);

  // The ConfirmPasswordChangeHandler is added by the dialog, so no need to add
  // it here.

  content::WebUIDataSource::Add(profile, source);
}

ConfirmPasswordChangeUI::~ConfirmPasswordChangeUI() = default;

UrgentPasswordExpiryNotificationUI::UrgentPasswordExpiryNotificationUI(
    content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled));

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIUrgentPasswordExpiryNotificationHost);

  source->DisableTrustedTypesCSP();

  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (attrs.has_expiration_time()) {
    const base::Time expiration_time = attrs.expiration_time();
    source->AddString("initialTitle", PasswordExpiryNotification::GetTitleText(
                                          expiration_time - base::Time::Now()));
    source->AddString("expirationTime",
                      base::NumberToString(expiration_time.ToJsTime()));
  }

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"body", IDS_PASSWORD_EXPIRY_CALL_TO_ACTION_CRITICAL},
      {"button", IDS_OK}};
  source->AddLocalizedStrings(kLocalizedStrings);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HTML);
  source->AddResourcePath("urgent_password_expiry_notification.js",
                          IDR_URGENT_PASSWORD_EXPIRY_NOTIFICATION_JS);

  web_ui->AddMessageHandler(
      std::make_unique<UrgentPasswordExpiryNotificationHandler>());

  content::WebUIDataSource::Add(profile, source);
}

UrgentPasswordExpiryNotificationUI::~UrgentPasswordExpiryNotificationUI() =
    default;

}  // namespace chromeos
