// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"

#include "ash/constants/ash_features.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/login/auth/challenge_response/cert_utils.h"
#include "chromeos/login/auth/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {
namespace {

std::vector<std::string> ConvertToVector(const base::Value& list) {
  std::vector<std::string> string_list;
  if (!list.is_list()) {
    return string_list;
  }

  for (const base::Value& value : list.GetList()) {
    if (value.is_string()) {
      string_list.push_back(value.GetString());
    }
  }

  return string_list;
}

bool ShouldDoSamlRedirect(const std::string& email) {
  if (email.empty())
    return false;

  // If there's a populated email, we must check first that this user is using
  // SAML in order to decide whether to show the interstitial page.
  AccountId account_id =
      user_manager::KnownUser(user_manager::UserManager::Get()->GetLocalState())
          .GetAccountId(email, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  return user && user->using_saml();
}

InSessionPasswordSyncManager* GetInSessionPasswordSyncManager() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  return InSessionPasswordSyncManagerFactory::GetForProfile(profile);
}

const char kMainElement[] = "$(\'main-element\').";

}  // namespace

LockScreenReauthHandler::LockScreenReauthHandler(const std::string& email)
    : email_(email) {}

LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleInitialize(const base::ListValue* value) {
  AllowJavascript();
  OnReauthDialogReadyForTesting();
  LoadAuthenticatorParam();
}

void LockScreenReauthHandler::HandleAuthenticatorLoaded(
    const base::ListValue* value) {
  VLOG(1) << "Authenticator finished loading";
  authenticator_state_ = AuthenticatorState::LOADED;

  if (waiting_caller_) {
    std::move(waiting_caller_).Run();
  }

  // Recreate the client cert usage observer, in order to track only the certs
  // used during the current sign-in attempt.
  extension_provided_client_cert_usage_observer_ =
      std::make_unique<LoginClientCertUsageObserver>();
}

void LockScreenReauthHandler::LoadAuthenticatorParam() {
  if (authenticator_state_ == AuthenticatorState::LOADING) {
    VLOG(1) << "Skip loading the Authenticator as it's already being loaded ";
    return;
  }

  authenticator_state_ = AuthenticatorState::LOADING;
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
          ->browser_policy_connector_ash()
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
  params.SetBoolean("doSamlRedirect", force_saml_redirect_for_testing_ ?
                    true : ShouldDoSamlRedirect(context.email));
  params.SetString("clientVersion", version_info::GetVersionNumber());
  params.SetBoolean("readOnlyEmail", true);

  CallJavascript("loadAuthenticator", params);
  if (features::IsNewLockScreenReauthLayoutEnabled()) {
    UpdateOrientationAndWidth();
  }
}

void LockScreenReauthHandler::UpdateOrientationAndWidth() {
  gfx::Size display = display::Screen::GetScreen()->GetPrimaryDisplay().size();
  bool is_horizontal = display.width() >= display.height();
  CallJavascript("setOrientation", base::Value(is_horizontal));

  auto* password_sync_manager = GetInSessionPasswordSyncManager();
  int width = password_sync_manager->GetDialogWidth();
  CallJavascript("setWidth", base::Value(width));
}

void LockScreenReauthHandler::CallJavascript(
    const std::string& function,
    const base::Value& params) {
  CallJavascriptFunction(std::string(kMainElement) + function, params);
}

void LockScreenReauthHandler::HandleCompleteAuthentication(
    const base::ListValue* params) {
  CHECK_EQ(params->GetList().size(), 6);
  std::string gaia_id, email, password;
  bool using_saml;
  ::login::StringList services = ::login::StringList();
  const base::DictionaryValue* password_attributes;
  gaia_id = params->GetList()[0].GetString();
  email = params->GetList()[1].GetString();
  password = params->GetList()[2].GetString();
  using_saml = params->GetList()[3].GetBool();
  services = ConvertToVector(params->GetList()[4]);
  params->GetList()[5].GetAsDictionary(&password_attributes);

  if (gaia::CanonicalizeEmail(email) != gaia::CanonicalizeEmail(email_)) {
    // The authenticated user email doesn't match the current user's email.
    CallJavascriptFunction(std::string(kMainElement) + "resetAuthenticator");
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

  pending_user_context_ = std::make_unique<UserContext>();
  if (!login::BuildUserContextForGaiaSignIn(
          login::GetUsertypeFromServicesString(services),
          user_manager::known_user::GetAccountId(email, gaia_id,
                                                 AccountType::GOOGLE),
          using_saml, false /* using_saml_api */, password,
          SamlPasswordAttributes::FromJs(*password_attributes),
          /*sync_trusted_vault_keys=*/absl::nullopt,
          *extension_provided_client_cert_usage_observer_,
          pending_user_context_.get(), nullptr)) {
    pending_user_context_.reset();
    NOTREACHED();
    return;
  }

  online_login_helper_->SetUserContext(std::move(pending_user_context_));
  online_login_helper_->RequestCookiesAndCompleteAuthentication();
}

void LockScreenReauthHandler::OnCookieWaitTimeout() {
  NOTREACHED() << "Cookie has timed out while attempting to login in.";
  auto* password_sync_manager = GetInSessionPasswordSyncManager();
  password_sync_manager->DismissDialog();
}

void LockScreenReauthHandler::OnReauthDialogReadyForTesting() {
  auto* password_sync_manager = GetInSessionPasswordSyncManager();
  password_sync_manager->OnReauthDialogReadyForTesting();
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
      InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager_->CheckCredentials(user_context,
                                           password_changed_callback);
}

void LockScreenReauthHandler::HandleUpdateUserPassword(
    const base::ListValue* value) {
  DCHECK(!value->GetList().empty());
  std::string old_password = value->GetList()[0].GetString();
  password_sync_manager_->UpdateUserPassword(old_password);
}

void LockScreenReauthHandler::ShowPasswordChangedScreen() {
  CallJavascriptFunction(std::string(kMainElement) + "passwordChanged");
}

void LockScreenReauthHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "initialize",
      base::BindRepeating(&LockScreenReauthHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));

  web_ui()->RegisterDeprecatedMessageCallback(
      "authenticatorLoaded",
      base::BindRepeating(&LockScreenReauthHandler::HandleAuthenticatorLoaded,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "completeAuthentication",
      base::BindRepeating(
          &LockScreenReauthHandler::HandleCompleteAuthentication,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "updateUserPassword",
      base::BindRepeating(&LockScreenReauthHandler::HandleUpdateUserPassword,
                          weak_factory_.GetWeakPtr()));
}

bool LockScreenReauthHandler::IsAuthenticatorLoaded(
    base::OnceClosure callback) {
  if (authenticator_state_ == AuthenticatorState::LOADED)
    return true;

  waiting_caller_ = std::move(callback);
  return false;
}

}  // namespace chromeos
