// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"

#include "base/notreached.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/login/auth/challenge_response/cert_utils.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

std::vector<std::string> ConvertToVector(const base::ListValue* list) {
  std::vector<std::string> string_list;
  if (!list) {
    return string_list;
  }

  for (const base::Value& value : *list) {
    if (value.is_string()) {
      string_list.push_back(value.GetString());
    }
  }

  return string_list;
}

}  // namespace

LockScreenReauthHandler::LockScreenReauthHandler(const std::string& email)
    : email_(email) {}

LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleInitialize(const base::ListValue* value) {
  LoadAuthenticatorParam();
}

void LockScreenReauthHandler::HandleAuthenticatorLoaded(
    const base::ListValue* value) {
  VLOG(1) << "Authenticator finished loading";
  authenticator_being_loaded_ = false;
  // Recreate the client cert usage observer, in order to track only the certs
  // used during the current sign-in attempt.
  extension_provided_client_cert_usage_observer_ =
      std::make_unique<LoginClientCertUsageObserver>();
}

void LockScreenReauthHandler::LoadAuthenticatorParam() {
  if (authenticator_being_loaded_) {
    VLOG(1) << "Skip loading the Authenticator as it's already being loaded ";
    return;
  }

  authenticator_being_loaded_ = true;
  login::GaiaContext context;
  context.force_reload = true;
  context.email = email_;

  std::string gaia_id;
  if (!context.email.empty() &&
      user_manager::known_user::FindGaiaID(
          AccountId::FromUserEmail(context.email), &gaia_id)) {
    context.gaia_id = gaia_id;
  }

  if (!context.email.empty()) {
    context.gaps_cookie = user_manager::known_user::GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  LoadGaia(context);
}

void LockScreenReauthHandler::LoadGaia(const login::GaiaContext& context) {
  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  signin_partition_manager->StartSigninSession(
      web_ui()->GetWebContents(),
      base::BindOnce(&LockScreenReauthHandler::LoadGaiaWithPartition,
                     weak_factory_.GetWeakPtr(), context));
}

void LockScreenReauthHandler::LoadGaiaWithPartition(
    const login::GaiaContext& context,
    const std::string& partition_name) {
  auto callback = base::BindOnce(
      &LockScreenReauthHandler::OnSetCookieForLoadGaiaWithPartition,
      weak_factory_.GetWeakPtr(), context, partition_name);
  if (context.gaps_cookie.empty()) {
    std::move(callback).Run(net::CookieAccessResult());
    return;
  }

  // When the network service is enabled the webRequest API doesn't allow
  // modification of the cookie header. So manually write the GAPS cookie into
  // the CookieManager.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  login::SetCookieForPartition(context, signin_partition_manager,
                               std::move(callback));
}

void LockScreenReauthHandler::OnSetCookieForLoadGaiaWithPartition(
    const login::GaiaContext& context,
    const std::string& partition_name,
    net::CookieAccessResult result) {
  base::DictionaryValue params;

  params.SetString("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  params.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  params.SetString("clientId",
                   GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.SetBoolean("dontResizeNonEmbeddedPages", false);
  params.SetBoolean("enableGaiaActionButtons", false);

  std::string enterprise_enrollment_domain(
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetEnterpriseEnrollmentDomain());

  if (enterprise_enrollment_domain.empty()) {
    enterprise_enrollment_domain = gaia::ExtractDomainName(context.email);
  }

  params.SetString("enterpriseEnrollmentDomain", enterprise_enrollment_domain);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!app_locale.empty());
  params.SetString("hl", app_locale);
  params.SetString("email", context.email);
  params.SetString("gaiaId", context.gaia_id);
  params.SetBoolean("extractSamlPasswordAttributes",
                    login::ExtractSamlPasswordAttributesEnabled());

  AllowJavascript();
  CallJavascriptFunction("$(\'main-element\').loadAuthenticator", params);
}

void LockScreenReauthHandler::HandleCompleteAuthentication(
    const base::ListValue* params) {
  CHECK_EQ(params->GetList().size(), 6);
  std::string gaia_id, email, password;
  bool using_saml;
  const base::ListValue* servicesList;
  ::login::StringList services = ::login::StringList();
  const base::DictionaryValue* password_attributes;
  gaia_id = params->GetList()[0].GetString();
  email = params->GetList()[1].GetString();
  password = params->GetList()[2].GetString();
  using_saml = params->GetList()[3].GetBool();
  params->GetList(4, &servicesList);
  services = ConvertToVector(servicesList);
  params->GetDictionary(5, &password_attributes);

  if (gaia::CanonicalizeEmail(email) != gaia::CanonicalizeEmail(email_)) {
    // The authenticated user email doesn't match the current user's email.
    CallJavascriptFunction("$(\'main-element\').resetAuthenticator");
    return;
  }

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());

  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  online_login_helper_ = std::make_unique<OnlineLoginHelper>(
      signin_partition_name_, signin_partition_manager,
      base::BindOnce(&LockScreenReauthHandler::OnCookieWaitTimeout,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&LockScreenReauthHandler::CheckCredentials,
                     weak_factory_.GetWeakPtr()));

  std::string error_message;
  pending_user_context_ = std::make_unique<UserContext>();
  if (!login::BuildUserContextForGaiaSignIn(
          login::GetUsertypeFromServicesString(services),
          user_manager::known_user::GetAccountId(email, gaia_id,
                                                 AccountType::GOOGLE),
          using_saml, false /* using_saml_api */, password,
          SamlPasswordAttributes::FromJs(*password_attributes),
          /*sync_trusted_vault_keys=*/base::nullopt,
          *extension_provided_client_cert_usage_observer_,
          pending_user_context_.get(), &error_message)) {
    pending_user_context_.reset();
    NOTREACHED();
    return;
  }

  online_login_helper_->SetUserContext(std::move(pending_user_context_));
  online_login_helper_->RequestCookiesAndCompleteAuthentication();
}

void LockScreenReauthHandler::OnCookieWaitTimeout() {
  NOTREACHED() << "Cookie has timed out while attempting to login in.";
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  chromeos::InSessionPasswordSyncManager* password_sync_manager =
      chromeos::InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager->DismissDialog();
}

void LockScreenReauthHandler::CheckCredentials(
    const UserContext& user_context) {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByAccountId(
      user_context.GetAccountId());
  if (!profile) {
    LOG(ERROR) << "Invalid account id";
    return;
  }
  auto password_changed_callback =
      base::BindRepeating(&LockScreenReauthHandler::ShowPasswordChangedScreen,
                          weak_factory_.GetWeakPtr());
  password_sync_manager_ =
      chromeos::InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager_->CheckCredentials(user_context,
                                           password_changed_callback);
}

void LockScreenReauthHandler::HandleUpdateUserPassword(
    const base::ListValue* value) {
  std::string old_password;
  value->GetString(0, &old_password);
  password_sync_manager_->UpdateUserPassword(old_password);
}

void LockScreenReauthHandler::ShowPasswordChangedScreen() {
  CallJavascriptFunction("$(\'main-element\').passwordChanged");
}

void LockScreenReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&LockScreenReauthHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "authenticatorLoaded",
      base::BindRepeating(&LockScreenReauthHandler::HandleAuthenticatorLoaded,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "completeAuthentication",
      base::BindRepeating(
          &LockScreenReauthHandler::HandleCompleteAuthentication,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "updateUserPassword",
      base::BindRepeating(&LockScreenReauthHandler::HandleUpdateUserPassword,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
