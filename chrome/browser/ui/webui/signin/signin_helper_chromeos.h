// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_HELPER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_HELPER_CHROMEOS_H_

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ash.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
class AccountManager;
}

namespace chromeos {

// A helper class for completing the inline login flow. Primarily, it is
// responsible for exchanging the auth code, obtained after a successful user
// sign in, for OAuth tokens and subsequently populating Chrome OS
// AccountManager with these tokens.
// This object is supposed to be used in a one-shot fashion and it deletes
// itself after its work is complete.
class SigninHelper : public GaiaAuthConsumer {
 public:
  SigninHelper(
      ash::AccountManager* account_manager,
      crosapi::AccountManagerAsh* account_manager_ash,
      const base::RepeatingClosure& close_dialog_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id);

  SigninHelper(const SigninHelper&) = delete;
  SigninHelper& operator=(const SigninHelper&) = delete;
  ~SigninHelper() override;

 protected:
  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  void UpsertAccount(const std::string& refresh_token);

  // Closes the inline login dialog and calls `Exit`.
  void CloseDialogAndExit();

  // Deletes this object.
  void Exit();

  ash::AccountManager* GetAccountManager();

  // Returns email address of the account being added.
  std::string GetEmail();

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();

 private:
  // A non-owning pointer to Chrome OS AccountManager.
  ash::AccountManager* const account_manager_;
  // A non-owning pointer to AccountManagerAsh.
  crosapi::AccountManagerAsh* const account_manager_ash_;
  // A closure to close the hosting dialog window.
  base::RepeatingClosure close_dialog_closure_;
  // The user's AccountKey for which |this| object has been created.
  account_manager::AccountKey account_key_;
  // The user's email for which |this| object has been created.
  const std::string email_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Used for exchanging auth code for OAuth tokens.
  GaiaAuthFetcher gaia_auth_fetcher_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_HELPER_CHROMEOS_H_
