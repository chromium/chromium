// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace {

// A helper class for completing the inline login flow. Primarily, it is
// responsible for exchanging the auth code, obtained after a successful user
// sign in, for OAuth tokens and subsequently populating Chrome OS
// AccountManager with these tokens.
// This object is supposed to be used in a one-shot fashion and it deletes
// itself after its work is complete.
class SigninHelper : public GaiaAuthConsumer {
 public:
  SigninHelper(
      Profile* profile,
      chromeos::AccountManager* account_manager,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code)
      : profile_(profile),
        account_manager_(account_manager),
        close_dialog_closure_(close_dialog_closure),
        email_(email),
        gaia_auth_fetcher_(this,
                           GaiaConstants::kChromeSource,
                           url_loader_factory) {
    account_key_ = chromeos::AccountManager::AccountKey{
        gaia_id, chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA};

    gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchange(auth_code);
  }

  ~SigninHelper() override = default;

  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override {
    // TODO(sinhak): Do not depend on Profile unnecessarily. A Profile should
    // call |AccountTrackerServiceFactory| for the list of accounts it wants to
    // pull from |AccountManager|, not the other way round. Remove this when we
    // release multi Profile on Chrome OS and have the infra in place to do
    // this.
    // Account info needs to be seeded before the OAuth2TokenService chain can
    // use it. Do this before anything else.
    AccountTrackerServiceFactory::GetForProfile(profile_)->SeedAccountInfo(
        account_key_.id, email_);

    account_manager_->UpsertToken(account_key_, result.refresh_token);

    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override {
    // TODO(sinhak): Display an error.
    close_dialog_closure_.Run();
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  // A non-owning pointer to Profile.
  Profile* const profile_;
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

void InlineLoginHandlerChromeOS::SetExtraInitParams(
    base::DictionaryValue& params) {
  const GaiaUrls* const gaia_urls = GaiaUrls::GetInstance();
  params.SetKey("service", base::Value("chromiumsync"));
  params.SetKey("isNewGaiaFlow", base::Value(true));
  params.SetKey("clientId", base::Value(gaia_urls->oauth2_chrome_client_id()));

  const GURL& url = gaia_urls->embedded_setup_chromeos_url(2U);
  params.SetKey("gaiaPath", base::Value(url.path().substr(1)));

  params.SetKey("constrained", base::Value("1"));
  params.SetKey("flow", base::Value("addaccount"));
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
  new SigninHelper(profile, account_manager, close_dialog_closure_,
                   account_manager->GetUrlLoaderFactory(), gaia_id, email,
                   auth_code);
}

}  // namespace chromeos
