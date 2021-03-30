// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/online_login_helper.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/cookie_access_result.h"

namespace chromeos {
class InSessionPasswordSyncManager;

class LockScreenReauthHandler : public content::WebUIMessageHandler {
 public:
  explicit LockScreenReauthHandler(const std::string& email);
  ~LockScreenReauthHandler() override;

  void RegisterMessages() override;

  void ShowPasswordChangedScreen();

  // WebUI message handlers.
  void HandleInitialize(const base::ListValue*);
  void HandleCompleteAuthentication(const base::ListValue*);
  void HandleAuthenticatorLoaded(const base::ListValue*);
  void HandleUpdateUserPassword(const base::ListValue*);

 private:
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

  void CheckCredentials(const UserContext& user_context);

  // True if the authenticator is still loading.
  bool authenticator_being_loaded_ = false;

  // User non-canonicalized email for display
  std::string email_;

  std::string signin_partition_name_;

  InSessionPasswordSyncManager* password_sync_manager_ = nullptr;

  std::unique_ptr<UserContext> pending_user_context_;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<OnlineLoginHelper> online_login_helper_;

  base::WeakPtrFactory<LockScreenReauthHandler> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
