// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace {

// Returns a base64-encoded hash code of "signin_scoped_device_id:gaia_id".
std::string GetAccountDeviceId(const std::string& signin_scoped_device_id,
                               const std::string& gaia_id) {
  std::string account_device_id;
  base::Base64Encode(
      crypto::SHA256HashString(signin_scoped_device_id + ":" + gaia_id),
      &account_device_id);
  return account_device_id;
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
    account_manager_->UpsertAccount(account_key_, email_, result.refresh_token);

    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override {
    // TODO(sinhak): Display an error.
    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
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
  // Used for exchanging auth code for OAuth tokens.
  GaiaAuthFetcher gaia_auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SigninHelper);
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
}

void InlineLoginHandlerChromeOS::SetExtraInitParams(
    base::DictionaryValue& params) {
  const GaiaUrls* const gaia_urls = GaiaUrls::GetInstance();
  params.SetKey("clientId", base::Value(gaia_urls->oauth2_chrome_client_id()));

  const GURL& url = gaia_urls->embedded_setup_chromeos_url(2U);
  params.SetKey("gaiaPath", base::Value(url.path().substr(1)));

  params.SetKey("constrained", base::Value("1"));
  params.SetKey("flow", base::Value("crosAddAccount"));
  params.SetBoolean("dontResizeNonEmbeddedPages", true);

  // For in-session login flows, request Gaia to ignore third party SAML IdP SSO
  // redirection policies (and redirect to SAML IdPs by default), otherwise some
  // managed users will not be able to login to Chrome OS at all. Please check
  // https://crbug.com/984525 and https://crbug.com/984525#c20 for more context.
  params.SetBoolean("ignoreCrOSIdpSetting", true);
}

void InlineLoginHandlerChromeOS::HandleAuthExtensionReadyMessage(
    const base::ListValue* args) {
  AllowJavascript();
  FireWebUIListener("showBackButton");
}

void InlineLoginHandlerChromeOS::CompleteLogin(const std::string& email,
                                               const std::string& password,
                                               const std::string& gaia_id,
                                               const std::string& auth_code,
                                               bool skip_for_now,
                                               bool trusted,
                                               bool trusted_found,
                                               bool choose_what_to_sync) {
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

  // SigninHelper deletes itself after its work is done.
  new SigninHelper(
      account_manager, close_dialog_closure_,
      account_manager->GetUrlLoaderFactory(), gaia_id, email, auth_code,
      GetAccountDeviceId(GetSigninScopedDeviceIdForProfile(profile), gaia_id));
}

void InlineLoginHandlerChromeOS::HandleDialogClose(
    const base::ListValue* args) {
  AllowJavascript();
  close_dialog_closure_.Run();
}

void InlineLoginHandlerChromeOS::ShowIncognitoAndCloseDialog(
    const base::ListValue* args) {
  chrome::NewIncognitoWindow(Profile::FromWebUI(web_ui()));
  close_dialog_closure_.Run();
}

}  // namespace chromeos
