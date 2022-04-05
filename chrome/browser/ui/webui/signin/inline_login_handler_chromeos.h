// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chrome/browser/ui/webui/signin/signin_helper_chromeos.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class PrefRegistrySimple;

namespace chromeos {

class InlineLoginHandlerChromeOS : public InlineLoginHandler {
 public:
  explicit InlineLoginHandlerChromeOS(
      const base::RepeatingClosure& close_dialog_closure);

  InlineLoginHandlerChromeOS(const InlineLoginHandlerChromeOS&) = delete;
  InlineLoginHandlerChromeOS& operator=(const InlineLoginHandlerChromeOS&) =
      delete;

  ~InlineLoginHandlerChromeOS() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // InlineLoginHandler overrides.
  void RegisterMessages() override;
  void SetExtraInitParams(base::Value::Dict& params) override;
  void CompleteLogin(const CompleteLoginParams& params) override;
  void HandleDialogClose(const base::ListValue* args) override;

 private:
  // A callback for `GetAccounts` invoked from `CompleteLogin`.
  void OnGetAccountsToCompleteLogin(
      const CompleteLoginParams& params,
      const std::vector<::account_manager::Account>& accounts);
  // Creates a `SigninHelper` instance to complete login of the new account.
  void CreateSigninHelper(const CompleteLoginParams& params,
                          std::unique_ptr<SigninHelper::ArcHelper> arc_helper);
  void ShowIncognitoAndCloseDialog(const base::Value::List& args);
  void GetAccountsInSession(const base::Value::List& args);
  void OnGetAccounts(const std::string& callback_id,
                     const std::vector<::account_manager::Account>& accounts);
  void GetAccountsNotAvailableInArc(const base::Value::List& args);
  void ContinueGetAccountsNotAvailableInArc(
      const std::string& callback_id,
      const std::vector<::account_manager::Account>& accounts);
  void FinishGetAccountsNotAvailableInArc(
      const std::string& callback_id,
      const std::vector<::account_manager::Account>& accounts,
      const base::flat_set<account_manager::Account>& arc_accounts);
  void MakeAvailableInArcAndCloseDialog(const base::Value::List& args);
  void HandleSkipWelcomePage(const base::Value::List& args);
  void OpenGuestWindowAndCloseDialog(const base::Value::List& args);
  // Show a screen to inform the user that adding `email` as a Secondary Account
  // is not allowed by `email`'s user policies.
  // See `SecondaryGoogleAccountUsage` for details.
  void ShowSigninBlockedErrorPage(const std::string& email,
                                  const std::string& hosted_domain);

  base::RepeatingClosure close_dialog_closure_;
  base::RepeatingCallback<void(const std::string&, const std::string&)>
      show_signin_blocked_error_;
  base::WeakPtrFactory<InlineLoginHandlerChromeOS> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
