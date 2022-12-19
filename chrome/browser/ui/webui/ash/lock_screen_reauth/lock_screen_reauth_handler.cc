// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/browser/ui/webui/ash/login/check_passwords_against_cryptohome_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/version/version_loader.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/user_manager/known_user.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

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

  // TODO(b/259675128): we shouldn't rely on `user->using_saml()` when deciding
  // which IdP page to show because this flag can be outdated. Admin could have
  // changed the IdP to GAIA since last authentication and we wouldn't know
  // about it.
  return user && user->using_saml();
}

Profile* GetActiveUserProfile() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  return profile;
}

std::string GetHostedDomain(const std::string& gaia_id) {
  Profile* profile = GetActiveUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::string();
  }
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
  return account_info.hosted_domain;
}

std::string GetSSOProfile() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetSSOProfile();
}

const char kMainElement[] = "$(\'main-element\').";
const char kIdpTestingDomain[] = "example.com";

}  // namespace

LockScreenReauthHandler::LockScreenReauthHandler(const std::string& email)
    : email_(email) {}

LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleInitialize(const base::Value::List& value) {
  AllowJavascript();
  OnReauthDialogReadyForTesting();
  LoadAuthenticatorParam();
}

void LockScreenReauthHandler::HandleAuthenticatorLoaded(
    const base::Value::List& value) {
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

  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (!context.email.empty()) {
    if (const std::string* gaia_id =
            known_user.FindGaiaID(AccountId::FromUserEmail(context.email))) {
      context.gaia_id = *gaia_id;
    }

    context.gaps_cookie = known_user.GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  LoadGaia(context);
}

void LockScreenReauthHandler::LoadGaia(const login::GaiaContext& context) {
  LOG_ASSERT(Profile::FromWebUI(web_ui()) ==
             ProfileHelper::Get()->GetLockScreenProfile());
  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  // TODO(http://crbug/1348126): we should also close signin session after the
  // flow is finished.
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
  base::Value::Dict params;

  params.Set("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  params.Set("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  params.Set("clientId", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.Set("dontResizeNonEmbeddedPages", false);
  params.Set("enableGaiaActionButtons", false);

  std::string hosted_domain = GetHostedDomain(context.gaia_id);

  if (hosted_domain.empty()) {
    LOG(ERROR) << "Couldn't get hosted_domain from account info.";
    params.Set("doSamlRedirect", force_saml_redirect_for_testing_);
  } else {
    params.Set("enterpriseEnrollmentDomain", force_saml_redirect_for_testing_
                                                 ? kIdpTestingDomain
                                                 : hosted_domain);
    params.Set("doSamlRedirect", force_saml_redirect_for_testing_ ||
                                     ShouldDoSamlRedirect(context.email));
  }

  const std::string sso_profile(GetSSOProfile());
  if (!sso_profile.empty()) {
    params.Set("ssoProfile", sso_profile);
  }

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!app_locale.empty());
  params.Set("hl", app_locale);
  params.Set("email", context.email);
  params.Set("gaiaId", context.gaia_id);
  params.Set("extractSamlPasswordAttributes",
             login::ExtractSamlPasswordAttributesEnabled());
  params.Set("clientVersion", version_info::GetVersionNumber());
  params.Set("readOnlyEmail", true);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->IsManagedPreference(
          prefs::kUrlParameterToAutofillSAMLUsername)) {
    params.Set(
        "urlParameterToAutofillSAMLUsername",
        local_state->GetString(prefs::kUrlParameterToAutofillSAMLUsername));
  }

  CallJavascript("loadAuthenticator", params);
  if (features::IsNewLockScreenReauthLayoutEnabled()) {
    UpdateOrientationAndWidth();
  }
}

void LockScreenReauthHandler::UpdateOrientationAndWidth() {
  gfx::Size display = display::Screen::GetScreen()->GetPrimaryDisplay().size();
  bool is_horizontal = display.width() >= display.height();
  CallJavascript("setOrientation", is_horizontal);
  const LockScreenStartReauthDialog* lock_screen_online_reauth_dialog =
      LockScreenStartReauthDialog::GetInstance();
  int width = lock_screen_online_reauth_dialog->GetDialogWidth();
  CallJavascript("setWidth", width);
}

void LockScreenReauthHandler::CallJavascript(const std::string& function,
                                             base::ValueView params) {
  CallJavascriptFunction(std::string(kMainElement) + function, params);
}

void LockScreenReauthHandler::HandleCompleteAuthentication(
    const base::Value::List& params) {
  CHECK_EQ(params.size(), 7u);
  std::string gaia_id, email, password;
  bool using_saml;
  gaia_id = params[0].GetString();
  email = params[1].GetString();
  password = params[2].GetString();
  auto scraped_saml_passwords =
      ::login::ConvertToStringList(params[3].GetList());
  using_saml = params[4].GetBool();
  const auto services = ::login::ConvertToStringList(params[5].GetList());
  const auto& password_attributes = params[6].GetDict();

  if (gaia::CanonicalizeEmail(email) != gaia::CanonicalizeEmail(email_)) {
    // The authenticated user email doesn't match the current user's email.
    CallJavascriptFunction(std::string(kMainElement) + "resetAuthenticator");
    return;
  }

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());

  OnlineLoginHelper::CompleteLoginCallback complete_login_callback =
      base::BindOnce(&LockScreenReauthHandler::CheckCredentials,
                     weak_factory_.GetWeakPtr());

  if (password.empty()) {
    CHECK_NE(scraped_saml_passwords.size(), 1u);
    complete_login_callback = base::BindOnce(
        &LockScreenReauthHandler::SamlConfirmPassword,
        weak_factory_.GetWeakPtr(), std::move(scraped_saml_passwords));
  }

  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));

  online_login_helper_ = std::make_unique<OnlineLoginHelper>(
      signin_partition_name_, signin_partition_manager,
      base::BindOnce(&LockScreenReauthHandler::OnCookieWaitTimeout,
                     weak_factory_.GetWeakPtr()),
      std::move(complete_login_callback));

  user_context_ = std::make_unique<UserContext>();
  if (!login::BuildUserContextForGaiaSignIn(
          login::GetUsertypeFromServicesString(services),
          AccountId::FromUserEmailGaiaId(gaia::CanonicalizeEmail(email),
                                         gaia_id),
          using_saml, false /* using_saml_api */, password,
          SamlPasswordAttributes::FromJs(password_attributes),
          /*sync_trusted_vault_keys=*/absl::nullopt,
          *extension_provided_client_cert_usage_observer_, user_context_.get(),
          nullptr)) {
    user_context_.reset();
    NOTREACHED();
    return;
  }

  online_login_helper_->SetUserContext(std::move(user_context_));
  online_login_helper_->RequestCookiesAndCompleteAuthentication();
}

void LockScreenReauthHandler::OnCookieWaitTimeout() {
  NOTREACHED() << "Cookie has timed out while attempting to login in.";
  LockScreenStartReauthDialog::Dismiss();
}

void LockScreenReauthHandler::OnReauthDialogReadyForTesting() {
  LockScreenStartReauthDialog* lock_screen_online_reauth_dialog =
      LockScreenStartReauthDialog::GetInstance();
  lock_screen_online_reauth_dialog->OnReadyForTesting();  // IN-TEST
}

void LockScreenReauthHandler::CheckCredentials(
    std::unique_ptr<UserContext> user_context) {
  Profile* profile =
      ProfileHelper::Get()->GetProfileByAccountId(user_context->GetAccountId());
  if (!profile) {
    LOG(ERROR) << "Invalid account id";
    return;
  }
  auto password_changed_callback =
      base::BindRepeating(&LockScreenReauthHandler::ShowPasswordChangedScreen,
                          weak_factory_.GetWeakPtr());
  password_sync_manager_ =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager_->CheckCredentials(*user_context,
                                           password_changed_callback);
}

void LockScreenReauthHandler::HandleUpdateUserPassword(
    const base::Value::List& value) {
  DCHECK(!value.empty());
  std::string old_password = value[0].GetString();
  password_sync_manager_->UpdateUserPassword(old_password);
}

void LockScreenReauthHandler::ShowPasswordChangedScreen() {
  CallJavascriptFunction(std::string(kMainElement) + "passwordChanged");
}

void LockScreenReauthHandler::ShowSamlConfirmPasswordScreen() {
  CallJavascript("showSamlConfirmPassword",
                 static_cast<int>(scraped_saml_passwords_.size()));
}

void LockScreenReauthHandler::HandleOnPasswordTyped(
    const base::Value::List& value) {
  OnPasswordTyped(value[0].GetString());
}

void LockScreenReauthHandler::OnPasswordTyped(const std::string& password) {
  if (scraped_saml_passwords_.empty() ||
      base::Contains(scraped_saml_passwords_, password)) {
    OnPasswordConfirmed(password);
    return;
  }
  ShowSamlConfirmPasswordScreen();
}

void LockScreenReauthHandler::OnPasswordConfirmed(const std::string& password) {
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  user_context_->SetKey(key);
  user_context_->SetPasswordKey(Key(password));
  CheckCredentials(std::move(user_context_));
  user_context_.reset();
  scraped_saml_passwords_.clear();
}

void LockScreenReauthHandler::SamlConfirmPassword(
    ::login::StringList scraped_saml_passwords,
    std::unique_ptr<UserContext> user_context) {
  scraped_saml_passwords_ = scraped_saml_passwords;
  user_context_ = std::move(user_context);

  if (!features::IsCheckPasswordsAgainstCryptohomeHelperEnabled() ||
      scraped_saml_passwords_.empty()) {
    ShowSamlConfirmPasswordScreen();
    return;
  }

  // TODO(https://crbug.com/1295294) Eliminate redundant cryptohome check.
  check_passwords_against_cryptohome_helper_ =
      std::make_unique<CheckPasswordsAgainstCryptohomeHelper>(
          *user_context_.get(), scraped_saml_passwords_,
          base::BindOnce(
              &LockScreenReauthHandler::ShowSamlConfirmPasswordScreen,
              weak_factory_.GetWeakPtr()),
          base::BindOnce(&LockScreenReauthHandler::OnPasswordConfirmed,
                         weak_factory_.GetWeakPtr()));
}

void LockScreenReauthHandler::HandleWebviewLoadAborted(int error_code) {
  if (error_code == net::ERR_INVALID_AUTH_CREDENTIALS) {
    // Silently ignore this error - it is used as an intermediate state for
    // committed interstitials (see https://crbug.com/1049349 for details).
    return;
  }

  if (error_code == net::ERR_ABORTED) {
    LOG(WARNING) << "Ignoring Gaia webview error: "
                 << net::ErrorToShortString(error_code);
    return;
  }

  LOG(ERROR) << "Gaia webview error: " << net::ErrorToShortString(error_code);
  LockScreenStartReauthDialog* lock_screen_online_reauth_dialog =
      LockScreenStartReauthDialog::GetInstance();
  lock_screen_online_reauth_dialog->OnWebviewLoadAborted();
}

void LockScreenReauthHandler::ReloadGaia() {
  CallJavascriptFunction(std::string(kMainElement) + "reloadAuthenticator");
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
  web_ui()->RegisterMessageCallback(
      "onPasswordTyped",
      base::BindRepeating(&LockScreenReauthHandler::HandleOnPasswordTyped,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterHandlerCallback(
      "webviewLoadAborted",
      base::BindRepeating(&LockScreenReauthHandler::HandleWebviewLoadAborted,
                          weak_factory_.GetWeakPtr()));
}

bool LockScreenReauthHandler::IsAuthenticatorLoaded(
    base::OnceClosure callback) {
  if (authenticator_state_ == AuthenticatorState::LOADED)
    return true;

  waiting_caller_ = std::move(callback);
  return false;
}

}  // namespace ash
