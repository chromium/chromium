// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/browser/chromeos/login/login_client_cert_usage_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/saml_challenge_key_handler.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "components/user_manager/user_type.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"

class AccountId;

namespace base {
class DictionaryValue;
}  // namespace base

namespace network {
class NSSTempCertsCacheChromeOS;
}

namespace chromeos {

class CookieWaiter;
class Key;
class SamlPasswordAttributes;
class SigninScreenHandler;
class UserContext;
class PublicSamlUrlFetcher;
class GaiaScreen;

class GaiaView {
 public:
  enum class GaiaPath {
    kDefault,
    kChildSignup,
    kChildSignin,
  };

  constexpr static StaticOobeScreenId kScreenId{"gaia-signin"};

  GaiaView() = default;
  virtual ~GaiaView() = default;

  // Decides whether an auth extension should be pre-loaded. If it should,
  // pre-loads it.
  virtual void MaybePreloadAuthExtension() = 0;

  virtual void DisableRestrictiveProxyCheckForTest() = 0;

  // Loads Gaia into the webview. Depending on internal state, the Gaia will
  // either be loaded immediately or after an asynchronous clean-up process that
  // cleans DNS cache and cookies. If available, |account_id| is used for
  // prefilling information.
  virtual void LoadGaiaAsync(const AccountId& account_id) = 0;

  virtual void LoadOfflineGaia(const AccountId& account_id) = 0;

  // Shows Gaia screen.
  virtual void Show() = 0;
  virtual void Hide() = 0;
  // Binds |screen| to the view.
  virtual void Bind(GaiaScreen* screen) = 0;
  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
  // Sets Gaia path for sign-in, child sign-in or child sign-up.
  virtual void SetGaiaPath(GaiaPath gaia_path) = 0;

  // Show sign-in screen for the given credentials. |services| is a list of
  // services returned by userInfo call as JSON array. Should be an empty array
  // for a regular user: "[]".
  virtual void ShowSigninScreenForTest(const std::string& username,
                                       const std::string& password,
                                       const std::string& services) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GaiaView);
};

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler : public BaseScreenHandler,
                          public GaiaView,
                          public NetworkPortalDetector::Observer,
                          public SecurityTokenPinDialogHost {
 public:
  using TView = GaiaView;

  // The possible modes that the Gaia signin screen can be in.
  enum GaiaScreenMode {
    // Default Gaia authentication will be used.
    GAIA_SCREEN_MODE_DEFAULT = 0,

    // Gaia offline mode will be used.
    GAIA_SCREEN_MODE_OFFLINE = 1,

    // An interstitial page will be used before SAML redirection.
    GAIA_SCREEN_MODE_SAML_INTERSTITIAL = 2,

    // Offline UI for Active Directory authentication.
    GAIA_SCREEN_MODE_AD = 3,
  };

  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR
  };

  GaiaScreenHandler(
      JSCallsContainer* js_calls_container,
      CoreOobeView* core_oobe_view,
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  ~GaiaScreenHandler() override;

  // GaiaView:
  void MaybePreloadAuthExtension() override;
  void DisableRestrictiveProxyCheckForTest() override;
  void LoadGaiaAsync(const AccountId& account_id) override;
  void LoadOfflineGaia(const AccountId& account_id) override;
  void Show() override;
  void Hide() override;
  void Bind(GaiaScreen* screen) override;
  void Unbind() override;
  void SetGaiaPath(GaiaPath gaia_path) override;
  void ShowSigninScreenForTest(const std::string& username,
                               const std::string& password,
                               const std::string& services) override;

  // SecurityTokenPinDialogHost:
  void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      security_token_pin::CodeType code_type,
      bool enable_user_input,
      security_token_pin::ErrorLabel error_label,
      int attempts_left,
      const base::Optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) override;
  void CloseSecurityTokenPinDialog() override;

  // Returns true if offline login mode was either required, or reported by the
  // WebUI (i.e. WebUI mignt not have completed transition to the new mode).
  bool IsOfflineLoginActive() const;

  void SetNextSamlChallengeKeyHandlerForTesting(
      std::unique_ptr<SamlChallengeKeyHandler> handler_for_test);

 private:
  // TODO (xiaoyinh): remove this dependency.
  friend class SigninScreenHandler;

  struct GaiaContext;

  void LoadGaia(const GaiaContext& context);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartition(const GaiaContext& context,
                             const std::string& partition_name);

  // Called after the GAPS cookie, if present, is added to the cookie store.
  void OnSetCookieForLoadGaiaWithPartition(const GaiaContext& context,
                                           const std::string& partition_name,
                                           net::CookieAccessResult result);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartitionAndVersionAndConsent(
      const GaiaContext& context,
      const std::string& partition_name,
      const std::string* platform_version,
      const bool* collect_stats_consent);

  // Sends request to reload Gaia. If |force_reload| is true, request
  // will be sent in any case, otherwise it will be sent only when Gaia is
  // not loading right now.
  void ReloadGaia(bool force_reload);

  // Turns offline idle detection on or off. Idle detection should only be on if
  // we're using the offline login page but the device is online.
  void MonitorOfflineIdle(bool is_online);

  // Show error UI at the end of GAIA flow when user is not allowlisted.
  void ShowAllowlistCheckFailedError();

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // NetworkPortalDetector::Observer implementation.
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // WebUI message handlers.
  void HandleWebviewLoadAborted(int error_code);
  void HandleCompleteAuthentication(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      bool using_saml,
      const ::login::StringList& services,
      const base::DictionaryValue* password_attributes);
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);

  void HandleCompleteAdAuthentication(const std::string& username,
                                      const std::string& password);

  void HandleCancelActiveDirectoryAuth();

  // Handles SAML/GAIA login flow metrics
  // is_third_party_idp == false means GAIA-based authentication
  void HandleUsingSAMLAPI(bool is_third_party_idp);
  void HandleRecordSAMLProvider(const std::string& x509certificate);
  void HandleScrapedPasswordCount(int password_count);
  void HandleScrapedPasswordVerificationFailed();
  void HandleSamlChallengeMachineKey(const std::string& callback_id,
                                     const std::string& url,
                                     const std::string& challenge);

  void HandleGaiaUIReady();

  void HandleIdentifierEntered(const std::string& account_identifier);

  void HandleAuthExtensionLoaded();
  void HandleShowAddUser(const base::ListValue* args);
  void HandleGetIsSamlUserPasswordless(const std::string& callback_id,
                                       const std::string& typed_email,
                                       const std::string& gaia_id);

  // Allows WebUI to control the login shelf's guest button visibility during
  // OOBE.
  void HandleShowGuestInOobe(bool show);

  // Called to notify whether the SAML sign-in is currently happening.
  void HandleSamlStateChanged(bool is_saml);
  // Called to deliver the result of the security token PIN request. Called with
  // an empty string when the request is canceled.
  void HandleSecurityTokenPinEntered(const std::string& user_input);

  void OnShowAddUser();

  // Really handles the complete login message.
  void DoCompleteLogin(const std::string& gaia_id,
                       const std::string& typed_email,
                       const std::string& password,
                       bool using_saml,
                       const SamlPasswordAttributes& password_attributes);

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(const base::Closure& on_clear_callback);
  void OnCookiesCleared(const base::Closure& on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Callback for AuthPolicyClient.
  void DoAdAuth(const std::string& username,
                const Key& key,
                authpolicy::ErrorType error,
                const authpolicy::ActiveDirectoryAccountInfo& account_info);

  // Attempts login for test.
  void SubmitLoginFormForTest();

  // Updates the member variable and UMA histogram indicating whether the
  // Chrome Credentials Passing API was used during SAML login.
  void SetSAMLPrincipalsAPIUsed(bool is_third_party_idp, bool is_api_used);

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |offline| is true when
  // offline version of the extension should be used.
  void LoadAuthExtension(bool force, bool offline);

  // TODO (antrim@): GaiaScreenHandler should implement
  // NetworkStateInformer::Observer.
  void UpdateState(NetworkError::ErrorReason reason);

  // TODO (antrim@): remove this dependency.
  void set_signin_screen_handler(SigninScreenHandler* handler) {
    signin_screen_handler_ = handler;
  }

  // Are we on a restrictive proxy?
  bool IsRestrictiveProxy() const;

  // Returns temporary unused device Id.
  std::string GetTemporaryDeviceId();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

  // Returns user canonical e-mail. Finds already used account alias, if
  // user has already signed in.
  AccountId GetAccountId(const std::string& authenticated_email,
                         const std::string& id,
                         const AccountType& account_type) const;

  // Records whether WebUI is currently in offline mode.
  void SetOfflineLoginIsActive(bool is_active);

  // Builds the UserContext with the information from the given Gaia user
  // sign-in. On failure, returns false and sets |error_message|.
  bool BuildUserContextForGaiaSignIn(
      user_manager::UserType user_type,
      const AccountId& account_id,
      bool using_saml,
      const std::string& password,
      const SamlPasswordAttributes& password_attributes,
      UserContext* user_context,
      std::string* error_message);

  void ContinueAuthenticationWhenCookiesAvailable();
  void OnGetCookiesForCompleteAuthentication(
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);

  void OnCookieWaitTimeout();

  bool is_security_token_pin_dialog_running() const {
    return !security_token_pin_dialog_closed_callback_.is_null();
  }

  // Assigns new SamlChallengeKeyHandler object or an object for testing to
  // |saml_challenge_key_handler_|.
  void CreateSamlChallengeKeyHandler();

  // Current state of Gaia frame.
  FrameState frame_state_ = FRAME_STATE_UNKNOWN;

  // Latest Gaia frame error.
  net::Error frame_error_ = net::OK;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  CoreOobeView* core_oobe_view_ = nullptr;

  // Account to pre-populate with.
  AccountId populated_account_id_;

  // Whether the handler has been initialized.
  bool initialized_ = false;

  bool show_on_init_ = false;

  // True if dns cache cleanup is done.
  bool dns_cleared_ = false;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_ = false;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_ = false;

  // If true, the sign-in screen will be shown when DNS cache and cookie
  // clean-up finish, and the handler is initialized (i.e. the web UI is ready).
  bool show_when_ready_ = false;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_ = false;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // This flag is set when user authenticated using the Chrome Credentials
  // Passing API (the login could happen via SAML or, with the current
  // server-side implementation, via Gaia).
  bool using_saml_api_ = false;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;
  // Test result of userInfo.
  std::string test_services_;
  bool test_expects_complete_login_ = false;

  // True if proxy doesn't allow access to google.com/generate_204.
  NetworkPortalDetector::CaptivePortalStatus captive_portal_status_ =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;

  bool disable_restrictive_proxy_check_for_test_ = false;

  // Non-owning ptr to SigninScreenHandler instance. Should not be used
  // in dtor.
  // TODO (antrim@): GaiaScreenHandler shouldn't communicate with
  // signin_screen_handler directly.
  SigninScreenHandler* signin_screen_handler_ = nullptr;

  // True if WebUI is currently displaying offline GAIA.
  bool offline_login_is_active_ = false;

  // True if the authentication extension is still loading.
  bool auth_extension_being_loaded_ = false;

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to
  // authenticate users against Active Directory server.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  // Makes untrusted authority certificates from device policy available for
  // client certificate discovery.
  std::unique_ptr<network::NSSTempCertsCacheChromeOS>
      untrusted_authority_certs_cache_;

  // The type of Gaia page to show.
  GaiaScreenMode screen_mode_ = GAIA_SCREEN_MODE_DEFAULT;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<chromeos::PublicSamlUrlFetcher> public_saml_url_fetcher_;

  // State of the security token PIN dialogs:

  // Whether this instance is currently registered as a host for showing the
  // security token PIN dialogs. (See PinDialogManager for the default host.)
  bool is_security_token_pin_enabled_ = false;
  // The callback to run when the user submits a non-empty input to the security
  // token PIN dialog.
  // Is non-empty iff the dialog is active and the input wasn't sent yet.
  SecurityTokenPinEnteredCallback security_token_pin_entered_callback_;
  // The callback to run when the security token PIN dialog gets closed - either
  // due to the user canceling the dialog or the whole sign-in attempt being
  // aborted.
  // Is non-empty iff the dialog is active.
  SecurityTokenPinDialogClosedCallback
      security_token_pin_dialog_closed_callback_;
  // Whether the PIN dialog shown during the current authentication attempt was
  // canceled by the user.
  bool was_security_token_pin_canceled_ = false;

  GaiaPath gaia_path_ = GaiaPath::kDefault;

  bool hidden_ = true;

  std::string signin_partition_name_;

  // Handler for |samlChallengeMachineKey| request.
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_;
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_for_test_;

  // Connection to the CookieManager that signals when the GAIA cookies change.
  std::unique_ptr<CookieWaiter> oauth_code_waiter_;
  std::unique_ptr<UserContext> pending_user_context_;

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GaiaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
