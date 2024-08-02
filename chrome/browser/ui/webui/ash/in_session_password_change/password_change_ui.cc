// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_ui.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/password_expiry_notification.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_dialogs.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_handler.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/urgent_password_expiry_notification_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/password_change_resources.h"
#include "chrome/grit/password_change_resources_map.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
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

namespace ash {

namespace {

std::string GetPasswordChangeUrl(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSamlPasswordChangeUrl)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kSamlPasswordChangeUrl);
  }

  const policy::UserCloudPolicyManagerAsh* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
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

bool PasswordChangeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(ash::prefs::kSamlInSessionPasswordChangeEnabled);
}

PasswordChangeUI::PasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPasswordChangeHost);
  ash::EnableTrustedTypesCSP(source);

  const std::string password_change_url = GetPasswordChangeUrl(profile);
  web_ui->AddMessageHandler(
      std::make_unique<PasswordChangeHandler>(password_change_url));

  source->AddString("hostedHeader", GetHostedHeaderText(password_change_url));
  source->UseStringsJs();

  source->AddResourcePaths(
      base::make_span(kPasswordChangeResources, kPasswordChangeResourcesSize));
  source->SetDefaultResource(IDR_PASSWORD_CHANGE_PASSWORD_CHANGE_APP_HTML);

  // Add Gaia Authenticator resources
  source->AddResourcePaths(
      base::make_span(kGaiaAuthHostResources, kGaiaAuthHostResourcesSize));
}

PasswordChangeUI::~PasswordChangeUI() = default;

bool ConfirmPasswordChangeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(ash::prefs::kSamlInSessionPasswordChangeEnabled);
}

ConfirmPasswordChangeUI::ConfirmPasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIConfirmPasswordChangeHost);
  ash::EnableTrustedTypesCSP(source);

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

  source->AddResourcePaths(
      base::make_span(kPasswordChangeResources, kPasswordChangeResourcesSize));
  source->SetDefaultResource(
      IDR_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_APP_HTML);

  // The ConfirmPasswordChangeHandler is added by the dialog, so no need to add
  // it here.
}

ConfirmPasswordChangeUI::~ConfirmPasswordChangeUI() = default;

bool UrgentPasswordExpiryNotificationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(ash::prefs::kSamlInSessionPasswordChangeEnabled);
}

UrgentPasswordExpiryNotificationUI::UrgentPasswordExpiryNotificationUI(
    content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIUrgentPasswordExpiryNotificationHost);
  ash::EnableTrustedTypesCSP(source);

  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (attrs.has_expiration_time()) {
    const base::Time expiration_time = attrs.expiration_time();
    source->AddString("initialTitle", PasswordExpiryNotification::GetTitleText(
                                          expiration_time - base::Time::Now()));
    source->AddString(
        "expirationTime",
        base::NumberToString(expiration_time.InMillisecondsFSinceUnixEpoch()));
  }

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"body", IDS_PASSWORD_EXPIRY_CALL_TO_ACTION_CRITICAL},
      {"button", IDS_OK}};
  source->AddLocalizedStrings(kLocalizedStrings);

  source->UseStringsJs();

  source->AddResourcePaths(
      base::make_span(kPasswordChangeResources, kPasswordChangeResourcesSize));
  source->SetDefaultResource(
      IDR_PASSWORD_CHANGE_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HTML);

  web_ui->AddMessageHandler(
      std::make_unique<UrgentPasswordExpiryNotificationHandler>());
}

UrgentPasswordExpiryNotificationUI::~UrgentPasswordExpiryNotificationUI() =
    default;

}  // namespace ash
