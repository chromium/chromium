// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_LOGIN_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_LOGIN_HELPER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/login_client_cert_usage_observer.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class SyncTrustedVaultKeys;
class UserContext;

namespace login {

// A class that's used to specify the way how Gaia should be loaded.
struct GaiaContext {
  GaiaContext();
  GaiaContext(GaiaContext const&);
  // Forces Gaia to reload.
  bool force_reload = false;

  // Email of the current user.
  std::string email;

  // GAIA ID of the current user.
  std::string gaia_id;

  // GAPS cookie.
  std::string gaps_cookie;
};

using LoadGaiaWithPartition = base::OnceCallback<void(const std::string&)>;

using OnSetCookieForLoadGaiaWithPartition =
    base::OnceCallback<void(::net::CookieAccessResult)>;

// Return whether the InSession Password Change feature is enabled.
bool ExtractSamlPasswordAttributesEnabled();

// Return Signin Session callback
base::OnceClosure GetStartSigninSession(::content::WebUI* web_ui,
                                        LoadGaiaWithPartition callback);

// Callback that set GAPS cookie for the partition.
void SetCookieForPartition(
    const login::GaiaContext& context,
    login::SigninPartitionManager* signin_partition_manager,
    OnSetCookieForLoadGaiaWithPartition callback);

// Return whether the user is regular user or child user.
user_manager::UserType GetUsertypeFromServicesString(
    const ::login::StringList& services);

// Builds the UserContext with the information from the given Gaia user
// sign-in. On failure, returns false and sets |error_message|.
bool BuildUserContextForGaiaSignIn(
    user_manager::UserType user_type,
    const AccountId& account_id,
    bool using_saml,
    bool using_saml_api,
    const std::string& password,
    const SamlPasswordAttributes& password_attributes,
    const absl::optional<SyncTrustedVaultKeys>& sync_trusted_vault_keys,
    const LoginClientCertUsageObserver&
        extension_provided_client_cert_usage_observer,
    UserContext* user_context,
    SigninError* error);

}  // namespace login

// This class will be used in authenticating Gaia and SAML users in loginscreen
// and SAML users in lockscreen.
class OnlineLoginHelper : public network::mojom::CookieChangeListener {
 public:
  using OnCookieTimeoutCallback = base::OnceCallback<void(void)>;
  using CompleteLoginCallback =
      base::OnceCallback<void(std::unique_ptr<UserContext>)>;

  explicit OnlineLoginHelper(
      std::string signin_partition_name,
      login::SigninPartitionManager* signin_partition_manager,
      OnCookieTimeoutCallback on_cookie_timeout_callback,
      CompleteLoginCallback complete_login_callback);

  OnlineLoginHelper(const OnlineLoginHelper&) = delete;
  OnlineLoginHelper& operator=(const OnlineLoginHelper&) = delete;

  ~OnlineLoginHelper() override;

  void SetUserContext(std::unique_ptr<UserContext> pending_user_context);

  void RequestCookiesAndCompleteAuthentication();

 private:
  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  void OnCookieWaitTimeout();

  void OnGetCookiesForCompleteAuthentication(
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);

  std::string signin_partition_name_;

  raw_ptr<login::SigninPartitionManager, ExperimentalAsh>
      signin_partition_manager_;

  // Connection to the CookieManager that signals when the GAIA cookies change.
  mojo::Receiver<network::mojom::CookieChangeListener> oauth_code_listener_{
      this};

  std::unique_ptr<UserContext> pending_user_context_;

  std::unique_ptr<base::OneShotTimer> cookie_waiting_timer_;

  OnCookieTimeoutCallback on_cookie_timeout_callback_;

  CompleteLoginCallback complete_login_callback_;

  base::WeakPtrFactory<OnlineLoginHelper> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_LOGIN_HELPER_H_
