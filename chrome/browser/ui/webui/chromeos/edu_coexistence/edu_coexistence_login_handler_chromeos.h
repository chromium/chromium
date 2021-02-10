// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_EDU_COEXISTENCE_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_EDU_COEXISTENCE_LOGIN_HANDLER_CHROMEOS_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {

// Handler for EDU account login flow.
class EduCoexistenceLoginHandler : public content::WebUIMessageHandler,
                                   public signin::IdentityManager::Observer {
 public:
  explicit EduCoexistenceLoginHandler(
      const base::RepeatingClosure& close_dialog_closure);
  EduCoexistenceLoginHandler(const base::RepeatingClosure& close_dialog_closure,
                             signin::IdentityManager* identity_manager);
  EduCoexistenceLoginHandler(const EduCoexistenceLoginHandler&) = delete;
  EduCoexistenceLoginHandler& operator=(const EduCoexistenceLoginHandler&) =
      delete;
  ~EduCoexistenceLoginHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // chromeos::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // Callback for PrimaryAccountAccessTokenFetcher.
  void OnOAuthAccessTokensFetched(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo info);

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

  bool in_error_state() const { return in_error_state_; }

 private:
  // Registered WebUi Message handlers.
  void InitializeEduArgs(const base::ListValue* args);
  void SendInitializeEduArgs();
  void ConsentValid(const base::ListValue* args);
  void ConsentLogged(const base::ListValue* args);
  void OnError(const base::ListValue* args);

  // Used for getting child access token.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  base::RepeatingClosure close_dialog_closure_;

  base::Optional<signin::AccessTokenInfo> oauth_access_token_;
  base::Optional<std::string> initialize_edu_args_callback_;

  std::string edu_account_email_;

  // Callback id back to the javascript when an account has been successfully
  // added.
  std::string account_added_callback_;

  // The terms of service version number.
  std::string terms_of_service_version_number_;

  signin::IdentityManager* const identity_manager_;

  // |in_error_state_| boolean tracks whether an error has occurred.
  // The error could happen when trying to access OAuth tokens.
  // The error could be reported from the online flow through the
  // |EduCoexistenceLoginHandler::OnError| call.
  // If the object is in error state and |InitializeEduArgs| is called, this
  // class will notify the ui to show the error screen through calling the
  // "on-error" webui javascript listener.
  // The other message callbacks can't be called from js in error state.
  bool in_error_state_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_EDU_COEXISTENCE_LOGIN_HANDLER_CHROMEOS_H_
