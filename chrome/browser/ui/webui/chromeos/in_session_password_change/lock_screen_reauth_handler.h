// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "base/values.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/online_login_helper.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/cookie_access_result.h"

namespace chromeos {

class LockScreenReauthHandler : public content::WebUIMessageHandler {
 public:
  explicit LockScreenReauthHandler(const std::string& email);
  ~LockScreenReauthHandler() override;

  void RegisterMessages() override;

  void ShowPasswordChangedScreen();

  void ReloadGaia();

  // WebUI message handlers.
  void HandleInitialize(const base::Value::List&);
  void HandleCompleteAuthentication(const base::Value::List&);
  void HandleAuthenticatorLoaded(const base::Value::List&);
  void HandleUpdateUserPassword(const base::Value::List&);

  bool IsAuthenticatorLoaded(base::OnceClosure callback);

  void force_saml_redirect_for_testing() {
    force_saml_redirect_for_testing_ = true;
  }

 private:
  enum class AuthenticatorState {
    NOT_LOADED,
    LOADING,
    LOADED
  };

  void LoadAuthenticatorParam();

  void LoadGaia(const login::GaiaContext& context);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartition(const login::GaiaContext& context,
                             const std::string& partition_name);

  // Called after the GAPS cookie, if present, is added to the cookie store.
  void OnSetCookieForLoadGaiaWithPartition(const login::GaiaContext& context,
                                           const std::string& partition_name,
                                           net::CookieAccessResult result);

  void OnCookieWaitTimeout();

  void OnReauthDialogReadyForTesting();

  void CheckCredentials(std::unique_ptr<UserContext> user_context);

  void UpdateOrientationAndWidth();

  void CallJavascript(const std::string& function,
                      const base::Value& params);

  AuthenticatorState authenticator_state_ = AuthenticatorState::NOT_LOADED;

  // For testing only. Forces SAML redirect regardless of email.
  bool force_saml_redirect_for_testing_ = false;

  // User non-canonicalized email for display
  std::string email_;

  std::string signin_partition_name_;

  InSessionPasswordSyncManager* password_sync_manager_ = nullptr;

  std::unique_ptr<UserContext> pending_user_context_;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<OnlineLoginHelper> online_login_helper_;

  // A test may be waiting for the authenticator to load.
  base::OnceClosure waiting_caller_;

  base::WeakPtrFactory<LockScreenReauthHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LockScreenReauthHandler;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
