// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_

#include <string>

#include "ash/components/account_manager/account_manager.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class PrefRegistrySimple;

namespace chromeos {

class InlineLoginHandlerChromeOS : public InlineLoginHandler {
 public:
  explicit InlineLoginHandlerChromeOS(
      const base::RepeatingClosure& close_dialog_closure);
  ~InlineLoginHandlerChromeOS() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // InlineLoginHandler overrides.
  void RegisterMessages() override;
  void SetExtraInitParams(base::DictionaryValue& params) override;
  void CompleteLogin(const std::string& email,
                     const std::string& password,
                     const std::string& gaia_id,
                     const std::string& auth_code,
                     bool skip_for_now,
                     bool trusted,
                     bool trusted_found,
                     bool choose_what_to_sync,
                     base::Value edu_login_params) override;
  void HandleDialogClose(const base::ListValue* args) override;

 private:
  void ShowIncognitoAndCloseDialog(const base::ListValue* args);
  void GetAccountsInSession(const base::ListValue* args);
  void OnGetAccounts(const std::string& callback_id,
                     const std::vector<::account_manager::Account>& accounts);
  void HandleSkipWelcomePage(const base::ListValue* args);

  base::RepeatingClosure close_dialog_closure_;
  base::WeakPtrFactory<InlineLoginHandlerChromeOS> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
