// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/child_accounts/secondary_account_consent_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/util/version_loader.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace {

constexpr char kCrosAddAccountFlow[] = "crosAddAccount";
constexpr char kCrosAddAccountEduFlow[] = "crosAddAccountEdu";

std::string AnonymizeAccountEmail(const std::string& email) {
  std::string result;
  base::Base64Encode(crypto::SHA256HashString(email), &result);
  return result + "@example.com";
}

// Returns a base64-encoded hash code of "signin_scoped_device_id:gaia_id".
std::string GetAccountDeviceId(const std::string& signin_scoped_device_id,
                               const std::string& gaia_id) {
  std::string account_device_id;
  base::Base64Encode(
      crypto::SHA256HashString(signin_scoped_device_id + ":" + gaia_id),
      &account_device_id);
  return account_device_id;
}

std::string GetInlineLoginFlowName(Profile* profile, const std::string* email) {
  DCHECK(profile);
  if (!profile->IsChild()) {
    return kCrosAddAccountFlow;
  }

  std::string primary_account_email =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
          .email;
  // If provided email is for primary account - it's a reauthentication, use
  // normal add account flow.
  if (email && gaia::AreEmailsSame(primary_account_email, *email)) {
    return kCrosAddAccountFlow;
  }

  // Child user is adding/reauthenticating a secondary account.
  return kCrosAddAccountEduFlow;
}

// A helper class for completing the inline login flow. Primarily, it is
// responsible for exchanging the auth code, obtained after a successful user
// sign in, for OAuth tokens and subsequently populating Chrome OS
// AccountManager with these tokens.
// This object is supposed to be used in a one-shot fashion and it deletes
// itself after its work is complete.
class SigninHelper : public GaiaAuthConsumer {
 public:
  SigninHelper(
      chromeos::AccountManager* account_manager,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id)
      : account_manager_(account_manager),
        close_dialog_closure_(close_dialog_closure),
        email_(email),
        url_loader_factory_(url_loader_factory),
        gaia_auth_fetcher_(this,
                           gaia::GaiaSource::kChrome,
                           url_loader_factory) {
    account_key_ = chromeos::AccountManager::AccountKey{
        gaia_id, chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA};

    DCHECK(!signin_scoped_device_id.empty());
    gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
        auth_code, signin_scoped_device_id);
  }

  ~SigninHelper() override = default;

 protected:
  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override {
    // Flow of control after this call:
    // |AccountManager::UpsertAccount| updates / inserts the account and calls
    // its |Observer|s, one of which is
    // |ProfileOAuth2TokenServiceDelegateChromeOS|.
    // |ProfileOAuth2TokenServiceDelegateChromeOS::OnTokenUpserted| seeds the
    // Gaia id and email id for this account in |AccountTrackerService| and
    // invokes |FireRefreshTokenAvailable|. This causes the account to propagate
    // throughout the Identity Service chain, including in
    // |AccountFetcherService|. |AccountFetcherService::OnRefreshTokenAvailable|
    // invokes |AccountTrackerService::StartTrackingAccount|, triggers a fetch
    // for the account information from Gaia and updates this information into
    // |AccountTrackerService|. At this point the account will be fully added to
    // the system.
    UpsertAccount(result.refresh_token);

    CloseDialogAndExit();
  }

  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override {
    // TODO(sinhak): Display an error.
    CloseDialogAndExit();
  }

  void UpsertAccount(const std::string& refresh_token) {
    account_manager_->UpsertAccount(account_key_, email_, refresh_token);
  }

  void CloseDialogAndExit() {
    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  chromeos::AccountManager* GetAccountManager() { return account_manager_; }

  const std::string GetEmail() { return email_; }

  const scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return url_loader_factory_;
  }

 private:
  // A non-owning pointer to Chrome OS AccountManager.
  chromeos::AccountManager* const account_manager_;
  // A closure to close the hosting dialog window.
  base::RepeatingClosure close_dialog_closure_;
  // The user's AccountKey for which |this| object has been created.
  chromeos::AccountManager::AccountKey account_key_;
  // The user's email for which |this| object has been created.
  const std::string email_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Used for exchanging auth code for OAuth tokens.
  GaiaAuthFetcher gaia_auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SigninHelper);
};

// A version of SigninHelper for child users. After obtaining OAuth token it
// logs the parental consent with provided parent id and rapt. After successful
// consent logging populates Chrome OS AccountManager with the token.
class ChildSigninHelper : public SigninHelper {
 public:
  ChildSigninHelper(
      chromeos::AccountManager* account_manager,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& re_auth_proof_token)
      : SigninHelper(account_manager,
                     close_dialog_closure,
                     url_loader_factory,
                     gaia_id,
                     email,
                     auth_code,
                     signin_scoped_device_id),
        identity_manager_(identity_manager),
        pref_service_(pref_service),
        parent_obfuscated_gaia_id_(parent_obfuscated_gaia_id),
        re_auth_proof_token_(re_auth_proof_token) {}
  ChildSigninHelper(const ChildSigninHelper&) = delete;
  ChildSigninHelper& operator=(const ChildSigninHelper&) = delete;
  ~ChildSigninHelper() override = default;

 protected:
  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override {
    // Log parental consent for secondary account addition. In case of success,
    // refresh token from |result| will be added to Account Manager in
    // |OnConsentLogged|.
    DCHECK(!secondary_account_consent_logger_);
    secondary_account_consent_logger_ =
        std::make_unique<SecondaryAccountConsentLogger>(
            identity_manager_, GetUrlLoaderFactory(), pref_service_, GetEmail(),
            parent_obfuscated_gaia_id_, re_auth_proof_token_,
            base::BindOnce(&ChildSigninHelper::OnConsentLogged,
                           weak_ptr_factory_.GetWeakPtr(),
                           result.refresh_token));
    secondary_account_consent_logger_->StartLogging();
  }

  void OnConsentLogged(const std::string& refresh_token,
                       SecondaryAccountConsentLogger::Result result) {
    UMA_HISTOGRAM_ENUMERATION("Signin.SecondaryAccountConsentLog", result);
    secondary_account_consent_logger_.reset();
    if (result == SecondaryAccountConsentLogger::Result::kSuccess) {
      // The EDU account has been added/reauthenticated. Mark migration to ARC++
      // as completed.
      if (arc::IsSecondaryAccountForChildEnabled()) {
        pref_service_->SetBoolean(prefs::kEduCoexistenceArcMigrationCompleted,
                                  true);
      }

      UpsertAccount(refresh_token);
    } else {
      LOG(ERROR) << "Could not log parent consent, the result was: "
                 << static_cast<int>(result);
      // TODO(anastasiian): send error to UI?
    }

    CloseDialogAndExit();
  }

 private:
  // Unowned pointer to identity manager.
  signin::IdentityManager* const identity_manager_;
  // Unowned pointer to pref service.
  PrefService* const pref_service_;
  const std::string parent_obfuscated_gaia_id_;
  const std::string re_auth_proof_token_;
  std::unique_ptr<SecondaryAccountConsentLogger>
      secondary_account_consent_logger_;
  base::WeakPtrFactory<ChildSigninHelper> weak_ptr_factory_{this};
};

}  // namespace

InlineLoginHandlerChromeOS::InlineLoginHandlerChromeOS(
    const base::RepeatingClosure& close_dialog_closure)
    : close_dialog_closure_(close_dialog_closure) {}

InlineLoginHandlerChromeOS::~InlineLoginHandlerChromeOS() = default;

void InlineLoginHandlerChromeOS::RegisterMessages() {
  InlineLoginHandler::RegisterMessages();

  web_ui()->RegisterMessageCallback(
      "showIncognito",
      base::BindRepeating(
          &InlineLoginHandlerChromeOS::ShowIncognitoAndCloseDialog,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAccounts",
      base::BindRepeating(&InlineLoginHandlerChromeOS::GetAccountsInSession,
                          base::Unretained(this)));
}

void InlineLoginHandlerChromeOS::SetExtraInitParams(
    base::DictionaryValue& params) {
  const GaiaUrls* const gaia_urls = GaiaUrls::GetInstance();
  params.SetKey("clientId", base::Value(gaia_urls->oauth2_chrome_client_id()));

  const GURL& url = gaia_urls->embedded_setup_chromeos_url(2U);
  params.SetKey("gaiaPath", base::Value(url.path().substr(1)));

  params.SetKey(
      "platformVersion",
      base::Value(version_loader::GetVersion(version_loader::VERSION_SHORT)));
  params.SetKey("constrained", base::Value("1"));
  params.SetKey("flow", base::Value(GetInlineLoginFlowName(
                            Profile::FromWebUI(web_ui()),
                            params.FindStringKey("email"))));
  params.SetBoolean("dontResizeNonEmbeddedPages", true);
  params.SetBoolean("enableGaiaActionButtons", true);

  // For in-session login flows, request Gaia to ignore third party SAML IdP SSO
  // redirection policies (and redirect to SAML IdPs by default), otherwise some
  // managed users will not be able to login to Chrome OS at all. Please check
  // https://crbug.com/984525 and https://crbug.com/984525#c20 for more context.
  params.SetBoolean("ignoreCrOSIdpSetting", true);
}

void InlineLoginHandlerChromeOS::CompleteLogin(const std::string& email,
                                               const std::string& password,
                                               const std::string& gaia_id,
                                               const std::string& auth_code,
                                               bool skip_for_now,
                                               bool trusted,
                                               bool trusted_found,
                                               bool choose_what_to_sync,
                                               base::Value edu_login_params) {
  CHECK(!auth_code.empty());
  CHECK(!gaia_id.empty());
  CHECK(!email.empty());

  // TODO(sinhak): Do not depend on Profile unnecessarily.
  Profile* profile = Profile::FromWebUI(web_ui());

  // TODO(sinhak): Do not depend on Profile unnecessarily. When multiprofile on
  // Chrome OS is released, get rid of |AccountManagerFactory| and get
  // AccountManager directly from |g_browser_process|.
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string primary_account_email =
      identity_manager
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
          .email;

  // Child user added a secondary account.
  if (profile->IsChild() &&
      !gaia::AreEmailsSame(primary_account_email, email)) {
    const std::string* rapt =
        edu_login_params.FindStringKey("reAuthProofToken");
    CHECK(rapt);
    const std::string* parentId =
        edu_login_params.FindStringKey("parentObfuscatedGaiaId");
    CHECK(parentId);
    InlineLoginDialogChromeOS::UpdateEduCoexistenceFlowResult(
        InlineLoginDialogChromeOS::EduCoexistenceFlowResult::kFlowCompleted);
    // ChildSigninHelper deletes itself after its work is done.
    new ChildSigninHelper(
        account_manager, close_dialog_closure_, profile->GetURLLoaderFactory(),
        gaia_id, email, auth_code,
        GetAccountDeviceId(GetSigninScopedDeviceIdForProfile(profile), gaia_id),
        identity_manager, profile->GetPrefs(), *parentId, *rapt);
    return;
  }

  // SigninHelper deletes itself after its work is done.
  new SigninHelper(
      account_manager, close_dialog_closure_, profile->GetURLLoaderFactory(),
      gaia_id, email, auth_code,
      GetAccountDeviceId(GetSigninScopedDeviceIdForProfile(profile), gaia_id));
}

void InlineLoginHandlerChromeOS::HandleDialogClose(
    const base::ListValue* args) {
  close_dialog_closure_.Run();
}

void InlineLoginHandlerChromeOS::ShowIncognitoAndCloseDialog(
    const base::ListValue* args) {
  chrome::NewIncognitoWindow(Profile::FromWebUI(web_ui()));
  close_dialog_closure_.Run();
}

void InlineLoginHandlerChromeOS::GetAccountsInSession(
    const base::ListValue* args) {
  const std::string& callback_id = args->GetList()[0].GetString();
  const Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());

  account_manager->GetAccounts(
      base::BindOnce(&InlineLoginHandlerChromeOS::OnGetAccounts,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void InlineLoginHandlerChromeOS::OnGetAccounts(
    const std::string& callback_id,
    const std::vector<AccountManager::Account>& accounts) {
  base::ListValue account_emails;
  for (const auto& account : accounts) {
    if (account.key.account_type ==
        account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY) {
      // Don't send Active Directory account email to Gaia.
      account_emails.Append(AnonymizeAccountEmail(account.raw_email));
    } else {
      account_emails.Append(account.raw_email);
    }
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                            std::move(account_emails));
}

}  // namespace chromeos
