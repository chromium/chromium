// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_IMPL_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/signin/ash/signin_helper.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "components/account_manager_core/account.h"

class PrefRegistrySimple;

namespace ash {

class InlineLoginHandlerImpl : public ::InlineLoginHandler {
 public:
  explicit InlineLoginHandlerImpl(
      const base::RepeatingClosure& close_dialog_closure);

  InlineLoginHandlerImpl(const InlineLoginHandlerImpl&) = delete;
  InlineLoginHandlerImpl& operator=(const InlineLoginHandlerImpl&) = delete;

  ~InlineLoginHandlerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // InlineLoginHandler overrides.
  void RegisterMessages() override;
  void SetExtraInitParams(base::Value::Dict& params) override;
  void CompleteLogin(const CompleteLoginParams& params) override;
  void HandleDialogClose(const base::Value::List& args) override;

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
  // Fires WebUIListener `show-signin-error-page` that would display an error
  // page informing the reason of the account not being added as a Secondary
  // account.
  // `email` is the email of the blocked account.
  // `hosted_domain` (optional) is the domain of the blocked account. It should
  // be provided iff signin is blocked by policy.
  void ShowSigninErrorPage(const std::string& email,
                           const std::string& hosted_domain);
  void GetDeviceId(const base::Value::List& args);

  // Email address provided at the start of the flow. Empty optional if no email
  // was provided.
  std::optional<std::string> initial_email_;
  base::RepeatingClosure close_dialog_closure_;
  base::RepeatingCallback<void(const std::string&, const std::string&)>
      show_signin_error_;
  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_IMPL_H_
