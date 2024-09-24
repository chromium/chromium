// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/check_passwords_against_cryptohome_helper.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/cookie_access_result.h"

namespace ash {

class LockScreenReauthManager;

class LockScreenReauthHandler : public content::WebUIMessageHandler {
 public:
  explicit LockScreenReauthHandler(const std::string& email);
  ~LockScreenReauthHandler() override;

  void RegisterMessages() override;

  void ShowPasswordChangedScreen();

  void ReloadGaia();

  // WebUI message handlers.
  void HandleStartOnlineAuth(const base::Value::List&);
  void HandleCompleteAuthentication(const base::Value::List&);
  void HandleAuthenticatorLoaded(const base::Value::List&);
  void HandleUpdateUserPassword(const base::Value::List&);
  void HandleOnPasswordTyped(const base::Value::List& value);
  void HandleWebviewLoadAborted(int error_code);
  void HandleGetDeviceId(const std::string& callback_id);

  bool IsAuthenticatorLoaded(base::OnceClosure callback);

 private:
  enum class AuthenticatorState { NOT_LOADED, LOADING, LOADED };

  void ShowSamlConfirmPasswordScreen();

  // Invoked when the user has successfully authenticated via SAML, the Chrome
  // Credentials Passing API was not used.
  // There are 2 different states that this method would be called with:
  // - Manual input password, where no password where scraped, then the
  // users will have a chance to choose their own password. Not necessarily the
  // same as their SAML password.
  // - Scraped password, where user confirm their password and it has to match
  // one of the scraped passwords.
  void OnPasswordTyped(const std::string& password);

  // Invoked when the user has successfully authenticated via SAML, the Chrome
  // Credentials Passing API was not used.
  // If multiple password were scraped AND
  // CheckPasswordsAgainstCryptohomeHelperEnabled is enabled then
  // CheckPasswordsAgainstCryptohomeHelper would be used, otherwise
  // `SAMLConfirmPassword` screen would be shown.
  void SamlConfirmPassword(::login::StringList scraped_saml_passwords,
                           std::unique_ptr<UserContext> user_context);

  // Invoked when the user has successfully authenticated via SAML, the Chrome
  // Credentials Passing API was not used.
  // There are 3 different states that this method would be called with:
  // -User confirmed their manual which not necessarily the same as their SAML
  // password.
  // -User confirmed which password among scraped password is the right one.
  // -Checking against cryptohome password was successful, where
  // CheckPasswordsAgainstCryptohomeHelper could successfully detect which one
  // is the user password among the list of scraped passwords.
  void OnPasswordConfirmed(const std::string& password);

  // Finish the authentication
  void FinishAuthentication(bool needs_saml_confirm_password,
                            ::login::StringList scraped_saml_password,
                            std::unique_ptr<UserContext> user_context,
                            login::GaiaCookiesData gaia_cookies);

  void LoadAuthenticatorParam(bool force_reauth_gaia_page = false);

  void LoadGaia(const login::GaiaContext& context,
                bool force_reauth_gaia_page = false);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartition(const login::GaiaContext& context,
                             bool force_reauth_gaia_page,
                             const std::string& partition_name);

  // Called after the GAPS cookie, if present, is added to the cookie store.
  void OnSetCookieForLoadGaiaWithPartition(const login::GaiaContext& context,
                                           bool force_reauth_gaia_page,
                                           const std::string& partition_name,
                                           net::CookieAccessResult result);

  void OnCookieWaitTimeout();

  void OnReauthDialogReadyForTesting();

  void CheckCredentials(std::unique_ptr<UserContext> user_context);

  void UpdateOrientationAndWidth();

  void CallJavascript(const std::string& function, base::ValueView params);

  AuthenticatorState authenticator_state_ = AuthenticatorState::NOT_LOADED;

  // User non-canonicalized email for display
  std::string email_;

  std::string signin_partition_name_;

  ::login::StringList scraped_saml_passwords_;

  raw_ptr<LockScreenReauthManager> lock_screen_reauth_manager_ = nullptr;

  std::unique_ptr<UserContext> user_context_;

  std::unique_ptr<CheckPasswordsAgainstCryptohomeHelper>
      check_passwords_against_cryptohome_helper_;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<GaiaCookieRetriever> gaia_cookie_retriever_;

  // A test may be waiting for the authenticator to load.
  base::OnceClosure waiting_caller_;

  base::WeakPtrFactory<LockScreenReauthHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_REAUTH_HANDLER_H_
