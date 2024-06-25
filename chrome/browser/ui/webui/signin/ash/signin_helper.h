// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_SIGNIN_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_SIGNIN_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/signin/ash/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

class AccountManager;
class AccountAppsAvailability;

// A helper class for completing the inline login flow. Primarily, it is
// responsible for exchanging the auth code, obtained after a successful user
// sign in, for OAuth tokens and subsequently populating Chrome OS
// AccountManager with these tokens.
// This object is supposed to be used in a one-shot fashion and it deletes
// itself after its work is complete.
class SigninHelper : public GaiaAuthConsumer {
 public:
  // A helper class that is responsible for setting the ARC availability
  // after account addition depending on the flags passed in the constructor.
  class ArcHelper {
   public:
    // If `is_account_addition` is `false` - the account is reauthenticated.
    ArcHelper(bool is_available_in_arc,
              bool is_account_addition,
              AccountAppsAvailability* account_apps_availability);
    ArcHelper(const ArcHelper&) = delete;
    ArcHelper& operator=(const ArcHelper&) = delete;
    virtual ~ArcHelper();

    // Sets the availability for the `account` in ARC.
    // Should be called only once after the account is added.
    void OnAccountAdded(const account_manager::Account& account);

    // Sets account availability in ARC.
    void SetIsAvailableInArc(bool is_available_in_arc);

    // Returns whether the account is available in ARC.
    bool IsAvailableInArc() const;

   private:
    bool is_available_in_arc_ = false;
    bool is_account_addition_ = false;
    // A non-owning pointer to AccountAppsAvailability which is a KeyedService
    // and should outlive this class.
    raw_ptr<AccountAppsAvailability> account_apps_availability_ = nullptr;
  };

  SigninHelper(
      account_manager::AccountManager* account_manager,
      crosapi::AccountManagerMojoService* account_manager_mojo_service,
      const base::RepeatingClosure& close_dialog_closure,
      const base::RepeatingCallback<
          void(const std::string&, const std::string&)>& show_signin_error,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<ArcHelper> arc_helper,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id);

  // Returns whether the account is available in ARC.
  bool IsAvailableInArc() const;

  SigninHelper(const SigninHelper&) = delete;
  SigninHelper& operator=(const SigninHelper&) = delete;
  ~SigninHelper() override;

 protected:
  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

  void UpsertAccount(const std::string& refresh_token);

  // Receives the callback for `GetSecondaryGoogleAccountUsage()`.
  void OnGetSecondaryGoogleAccountUsage(
      UserCloudSigninRestrictionPolicyFetcher::Status status,
      std::optional<std::string> policy_result,
      const std::string& hosted_domain);

  void OnGetSecondaryAccountAllowedInArcPolicy(
      UserCloudSigninRestrictionPolicyFetcher::Status status,
      std::optional<bool> policy_result);

  // Shows account sign-in blocked UI.
  void ShowSigninBlockedErrorPageAndExit(const std::string& hosted_domain);

  // Virtual for testing.
  virtual void RevokeGaiaTokenOnServer();

  // Closes the inline login dialog and calls `Exit()`.
  void CloseDialogAndExit();

  // Deletes this object.
  void Exit();

  account_manager::AccountManager* GetAccountManager();

  // Returns email address of the account being added.
  std::string GetEmail();

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();

 private:
  // Returns the account that must be auto-signed-in to the Main Profile in
  // Lacros. This is, when available, the account used to sign into the Chrome
  // OS session. This may be a Gaia account or a Microsoft Active Directory
  // account. This field will be null for Guest sessions, Managed Guest
  // sessions, Demo mode, and Kiosks.
  bool IsInitialPrimaryAccount();
  // Fetcher to get SecondaryGoogleAccountUsage policy value.
  std::unique_ptr<UserCloudSigninRestrictionPolicyFetcher> restriction_fetcher_;
  // The user's refresh token fetched in `this` object.
  std::string refresh_token_;
  // A non-owning pointer to Chrome OS AccountManager.
  const raw_ptr<account_manager::AccountManager> account_manager_;
  // A non-owning pointer to AccountManagerMojoService.
  const raw_ptr<crosapi::AccountManagerMojoService>
      account_manager_mojo_service_;
  // Sets the ARC availability
  // after account addition. Owned by this class.
  std::unique_ptr<ArcHelper> arc_helper_;
  // A closure to close the hosting dialog window.
  base::RepeatingClosure close_dialog_closure_;
  // A callback that shows the page of an enterprise account sign-in blocked by
  // policy.
  base::RepeatingCallback<void(const std::string&, const std::string&)>
      show_signin_error_;
  // The user's AccountKey for which `this` object has been created.
  account_manager::AccountKey account_key_;
  // The user's email for which `this` object has been created.
  const std::string email_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Used for exchanging auth code for OAuth tokens.
  GaiaAuthFetcher gaia_auth_fetcher_;

  base::WeakPtrFactory<SigninHelper> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_SIGNIN_HELPER_H_
