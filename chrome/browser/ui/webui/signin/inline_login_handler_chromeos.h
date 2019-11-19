// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace chromeos {

class InlineLoginHandlerChromeOS : public InlineLoginHandler {
 public:
  explicit InlineLoginHandlerChromeOS(
      const base::RepeatingClosure& close_dialog_closure);
  ~InlineLoginHandlerChromeOS() override;

  // InlineLoginHandler overrides.
  void RegisterMessages() override;
  void SetExtraInitParams(base::DictionaryValue& params) override;
  void HandleAuthExtensionReadyMessage(const base::ListValue* args) override;
  void CompleteLogin(const std::string& email,
                     const std::string& password,
                     const std::string& gaia_id,
                     const std::string& auth_code,
                     bool skip_for_now,
                     bool trusted,
                     bool trusted_found,
                     bool choose_what_to_sync) override;
  void HandleDialogClose(const base::ListValue* args) override;

 private:
  void ShowIncognitoAndCloseDialog(const base::ListValue* args);

  base::RepeatingClosure close_dialog_closure_;
  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
