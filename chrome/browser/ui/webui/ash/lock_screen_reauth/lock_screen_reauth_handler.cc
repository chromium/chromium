// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/browser/ui/webui/ash/login/check_passwords_against_cryptohome_helper.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/version/version_loader.h"
#include "components/account_id/account_id.h"
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
  // TODO(b/335388700): If automatic re-authentication start is configured we
  // have to skip any user verification notice page. For SAML this is currently
  // only possible with redirect endpoint. Once reauth endpoint enables this,
  // remove auto_start_reauth from this function.
  const PrefService* prefs =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetProfilePrefs();
  bool auto_start_reauth =
      prefs && prefs->GetBoolean(::prefs::kLockScreenAutoStartOnlineReauth);
  if (!auto_start_reauth) {
    return false;
  }

  if (email.empty()) {
    return false;
  }

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

std::string GetSSOProfile() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetSSOProfile();
}

std::string GetDeviceId(const user_manager::KnownUser& known_user) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(user) << "Could not find an active user for lock screen";

  std::string device_id = known_user.GetDeviceId(user->GetAccountId());
  if (device_id.empty()) {
    // TODO(http://b/311342008): Unify the error handling for missing device ids
    // post login. We should ideally CHECK() here.
    LOG(ERROR) << "Could not find a device id associated with this user";
    return base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  return device_id;
}

const char kMainElement[] = "$(\'main-element\').";

}  // namespace

LockScreenReauthHandler::LockScreenReauthHandler(const std::string& email)
    : email_(email) {}

LockScreenReauthHandler::~LockScreenReauthHandler() = default;

void LockScreenReauthHandler::HandleStartOnlineAuth(
    const base::Value::List& value) {
  AllowJavascript();
  OnReauthDialogReadyForTesting();

  CHECK_EQ(1u, value.size());
  const bool force_reauth_gaia_page = value[0].GetBool();
  LoadAuthenticatorParam(force_reauth_gaia_page);
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

void LockScreenReauthHandler::LoadAuthenticatorParam(
    const bool force_reauth_gaia_page) {
  if (authenticator_state_ == AuthenticatorState::LOADING) {
    VLOG(1) << "Skip loading the Authenticator as it's already being loaded ";
    return;
  }

  authenticator_state_ = AuthenticatorState::LOADING;
  login::GaiaContext context;
  context.email = email_;
  context.gaia_id = user_manager::UserManager::Get()
                        ->GetPrimaryUser()
                        ->GetAccountId()
                        .GetGaiaId();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (!context.email.empty()) {
    context.gaps_cookie = known_user.GetGAPSCookie(
        AccountId::FromUserEmail(gaia::CanonicalizeEmail(context.email)));
  }

  LoadGaia(context, force_reauth_gaia_page);
}

void LockScreenReauthHandler::LoadGaia(const login::GaiaContext& context,
                                       const bool force_reauth_gaia_page) {
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
                     weak_factory_.GetWeakPtr(), context,
                     force_reauth_gaia_page));
}

void LockScreenReauthHandler::LoadGaiaWithPartition(
    const login::GaiaContext& context,
    const bool force_reauth_gaia_page,
    const std::string& partition_name) {
  auto callback = base::BindOnce(
      &LockScreenReauthHandler::OnSetCookieForLoadGaiaWithPartition,
      weak_factory_.GetWeakPtr(), context, force_reauth_gaia_page,
      partition_name);
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
    const bool force_reauth_gaia_page,
    const std::string& partition_name,
    net::CookieAccessResult result) {
  base::Value::Dict params;

  params.Set("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  const GaiaUrls& gaia_urls = *GaiaUrls::GetInstance();
  params.Set("gaiaUrl", gaia_urls.gaia_url().spec());
  params.Set("clientId", gaia_urls.oauth2_chrome_client_id());

  bool do_saml_redirect =
      !force_reauth_gaia_page && ShouldDoSamlRedirect(context.email);
  params.Set("doSamlRedirect", do_saml_redirect);

  // Path without the leading slash, as expected by authenticator.js.
  const std::string default_gaia_path =
      gaia_urls.embedded_setup_chromeos_url().path().substr(1);
  params.Set("fallbackGaiaPath", default_gaia_path);
  if (do_saml_redirect) {
    params.Set("gaiaPath",
               gaia_urls.saml_redirect_chromeos_url().path().substr(1));
  } else if (!context.email.empty()) {
    params.Set("gaiaPath",
               gaia_urls.embedded_reauth_chromeos_url().path().substr(1));
  } else {
    params.Set("gaiaPath", default_gaia_path);
  }

  const std::string domain = enterprise_util::GetDomainFromEmail(context.email);
  if (!domain.empty()) {
    params.Set("enterpriseEnrollmentDomain", domain);
  } else {
    // TODO(b/332481266): add proper error handling.
    LOG(ERROR) << "Couldn't get domain for account.";
  }
  params.Set("enableGaiaActionButtons", !do_saml_redirect);
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

  // Retrieve ChallengeResponseKey from client certificates.
  std::optional<ChallengeResponseKey> challenge_response_key;
  if (using_saml && extension_provided_client_cert_usage_observer_ &&
      extension_provided_client_cert_usage_observer_->ClientCertsWereUsed()) {
    auto challenge_response_key_or_error = login::ExtractClientCertificates(
        *extension_provided_client_cert_usage_observer_);
    if (!challenge_response_key_or_error.has_value()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    challenge_response_key = challenge_response_key_or_error.value();
  }

  // Build UserContext.
  user_context_ = login::BuildUserContextForGaiaSignIn(
      login::GetUsertypeFromServicesString(services),
      AccountId::FromUserEmailGaiaId(gaia::CanonicalizeEmail(email), gaia_id),
      using_saml, false /* using_saml_api */, password,
      SamlPasswordAttributes::FromJs(password_attributes),
      /*sync_trusted_vault_keys=*/std::nullopt, challenge_response_key);

  // Create GaiaCookiesRetriever.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  gaia_cookie_retriever_ = std::make_unique<GaiaCookieRetriever>(
      signin_partition_name_, signin_partition_manager,
      base::BindOnce(&LockScreenReauthHandler::OnCookieWaitTimeout,
                     weak_factory_.GetWeakPtr()));

  // Create the callback that will be invoked once cookies are received.
  const bool needs_saml_confirm_password = password.empty();
  GaiaCookieRetriever::OnCookieRetrievedCallback finish_auth_callback =
      base::BindOnce(&LockScreenReauthHandler::FinishAuthentication,
                     weak_factory_.GetWeakPtr(), needs_saml_confirm_password,
                     std::move(scraped_saml_passwords),
                     std::move(user_context_));

  // Request cookies and proceed with authentication.
  gaia_cookie_retriever_->RetrieveCookies(std::move(finish_auth_callback));
}

void LockScreenReauthHandler::FinishAuthentication(
    bool needs_saml_confirm_password,
    ::login::StringList scraped_saml_passwords,
    std::unique_ptr<UserContext> user_context,
    login::GaiaCookiesData gaia_cookies) {
  gaia_cookies.TransferCookiesToUserContext(*user_context);

  if (needs_saml_confirm_password) {
    CHECK_NE(scraped_saml_passwords.size(), 1u);
    SamlConfirmPassword(scraped_saml_passwords, std::move(user_context));
  } else {
    CheckCredentials(std::move(user_context));
  }
}

void LockScreenReauthHandler::OnCookieWaitTimeout() {
  NOTREACHED_IN_MIGRATION()
      << "Cookie has timed out while attempting to login in.";
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
  lock_screen_reauth_manager_ =
      LockScreenReauthManagerFactory::GetForProfile(profile);
  CHECK(lock_screen_reauth_manager_);
  lock_screen_reauth_manager_->CheckCredentials(*user_context,
                                                password_changed_callback);
}

void LockScreenReauthHandler::HandleUpdateUserPassword(
    const base::Value::List& value) {
  DCHECK(!value.empty());
  std::string old_password = value[0].GetString();
  lock_screen_reauth_manager_->UpdateUserPassword(old_password);
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
  user_context_->SetSamlPassword(SamlPassword{password});
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

  // TODO(crbug.com/40214270) Eliminate redundant cryptohome check.
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
  if (error_code == net::ERR_BLOCKED_BY_ADMINISTRATOR) {
    // Ignore this error to let the user see the error screen for blocked sites.
    return;
  }

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

void LockScreenReauthHandler::HandleGetDeviceId(
    const std::string& callback_id) {
  if (!IsJavascriptAllowed()) {
    return;
  }

  user_manager::KnownUser known_user{g_browser_process->local_state()};
  ResolveJavascriptCallback(callback_id, GetDeviceId(known_user));
}

void LockScreenReauthHandler::ReloadGaia() {
  CallJavascriptFunction(std::string(kMainElement) + "reloadAuthenticator");
}

void LockScreenReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startOnlineAuth",
      base::BindRepeating(&LockScreenReauthHandler::HandleStartOnlineAuth,
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
  web_ui()->RegisterHandlerCallback(
      "getDeviceId",
      base::BindRepeating(&LockScreenReauthHandler::HandleGetDeviceId,
                          weak_factory_.GetWeakPtr()));
}

bool LockScreenReauthHandler::IsAuthenticatorLoaded(
    base::OnceClosure callback) {
  if (authenticator_state_ == AuthenticatorState::LOADED) {
    return true;
  }

  waiting_caller_ = std::move(callback);
  return false;
}

}  // namespace ash
