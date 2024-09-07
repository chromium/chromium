// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_SCREEN_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_client_cert_usage_observer.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/signin/authentication_flow_auto_reload_manager.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"

class AccountId;

namespace base {
class ElapsedTimer;
}  // namespace base

namespace network {
class NSSTempCertsCacheChromeOS;
}  // namespace network

namespace ash {

class PublicSamlUrlFetcher;
class ErrorScreensHistogramHelper;
class SamlChallengeKeyHandler;

class GaiaView {
 public:
  enum class GaiaLoginVariant {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    kUnknown = 0,
    kOobe = 1,
    kAddUser = 2,
    kOnlineSignin = 3,
    kMaxValue = kOnlineSignin
  };

  enum class PasswordlessSupportLevel {
    // Passwordless logins are not supported or password logins are enforced.
    kNone = 0,
    // Passwordless logins are supported for consumers only, but not for
    // enterprise users.
    kConsumersOnly,
    // Passwordless logins are supported for all users.
    kAll,
  };

  inline constexpr static StaticOobeScreenId kScreenId{"gaia-signin",
                                                       "GaiaSigninScreen"};

  GaiaView() = default;

  GaiaView(const GaiaView&) = delete;
  GaiaView& operator=(const GaiaView&) = delete;

  virtual ~GaiaView() = default;

  // Loads Gaia into the webview. Depending on internal state, the Gaia will
  // either be loaded immediately or after an asynchronous clean-up process that
  // cleans DNS cache and cookies. If available, `account_id` is used for
  // prefilling information.
  virtual void LoadGaiaAsync(const AccountId& account_id) = 0;

  // Shows Gaia screen.
  virtual void Show() = 0;
  virtual void Hide() = 0;
  // Reloads authenticator.
  virtual void ReloadGaiaAuthenticator() = 0;
  // Sets reauth request token in the URL, in order to get reauth proof token
  // for recovery.
  virtual void SetReauthRequestToken(
      const std::string& reauth_request_token) = 0;
  // Shows pop-up saying that enrollment is required for user's managed domain.
  virtual void ShowEnrollmentNudge(const std::string& email_domain) = 0;
  // Checks if user's email is allowlisted.
  virtual void CheckIfAllowlisted(const std::string& user_email) = 0;
  // Shows a page with loading animation on top of the Gaia screen.
  virtual void ToggleLoadingUI(bool is_shown) = 0;

  // Show sign-in screen for the given credentials. `services` is a list of
  // services returned by userInfo call as JSON array. Should be an empty array
  // for a regular user: "[]".
  virtual void ShowSigninScreenForTest(const std::string& username,
                                       const std::string& password,
                                       const std::string& services) = 0;
  virtual void SetQuickStartEntryPointVisibility(bool visible) = 0;
  // Sets if Gaia password is required during login. If the password is
  // required, Gaia passwordless login will be disallowed.
  virtual void SetIsGaiaPasswordRequired(bool is_required) = 0;

  // Reset authenticator.
  virtual void Reset() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<GaiaView> AsWeakPtr() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler final
    : public BaseScreenHandler,
      public GaiaView,
      public chromeos::SecurityTokenPinDialogHost,
      public HttpAuthDialog::Observer,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = GaiaView;

  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR,
    FRAME_STATE_BLOCKED
  };

  GaiaScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreen* error_screen);

  GaiaScreenHandler(const GaiaScreenHandler&) = delete;
  GaiaScreenHandler& operator=(const GaiaScreenHandler&) = delete;

  ~GaiaScreenHandler() override;

  // GaiaView:
  void LoadGaiaAsync(const AccountId& account_id) override;
  void Show() override;
  void Hide() override;
  void ReloadGaiaAuthenticator() override;
  void SetReauthRequestToken(const std::string& reauth_request_token) override;
  void ShowEnrollmentNudge(const std::string& email_domain) override;
  void CheckIfAllowlisted(const std::string& user_email) override;
  void ToggleLoadingUI(bool is_shown) override;

  void ShowSigninScreenForTest(const std::string& username,
                               const std::string& password,
                               const std::string& services) override;

  void SetQuickStartEntryPointVisibility(bool visible) override;
  void SetIsGaiaPasswordRequired(bool is_required) override;

  void Reset() override;
  base::WeakPtr<GaiaView> AsWeakPtr() override;

  // SecurityTokenPinDialogHost:
  void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      chromeos::security_token_pin::CodeType code_type,
      bool enable_user_input,
      chromeos::security_token_pin::ErrorLabel error_label,
      int attempts_left,
      const std::optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) override;
  void CloseSecurityTokenPinDialog() override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  // Returns the initial mode of the Gaia signin screen for a given user email
  // address. Note this also affects which Gaia endpoint is used.
  static WizardContext::GaiaScreenMode GetGaiaScreenMode(
      const std::string& email);

  void SetNextSamlChallengeKeyHandlerForTesting(
      std::unique_ptr<SamlChallengeKeyHandler> handler_for_test);

  // To avoid spurious error messages on flaky networks, the offline message is
  // only shown if the network is offline for a threshold number of seconds.
  // This method provides an ability to reduce the threshold to zero, allowing
  // the offline message to show instantaneously in tests. The threshold can
  // also be set to a high value to disable the offline message on slow
  // configurations like MSAN, where it otherwise triggers on every run.
  void set_offline_timeout_for_testing(base::TimeDelta offline_timeout) {
    offline_timeout_ = offline_timeout;
  }

  // TODO(https://issuetracker.google.com/292489063): Remove these methods to
  // query the frame state, and instead, allow registering callbacks or futures
  // to learn of the relevant state transitions e.g. with an Observer class.
  bool IsLoadedForTesting() const;
  bool IsNavigationBlockedForTesting() const;

  ash::AuthenticationFlowAutoReloadManager& GetAutoReloadManagerForTesting();

 private:
  void LoadGaia(const login::GaiaContext& context);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartition(const login::GaiaContext& context,
                             const std::string& partition_name);

  // Called after the GAPS cookie, if present, is added to the cookie store.
  void OnSetCookieForLoadGaiaWithPartition(const login::GaiaContext& context,
                                           const std::string& partition_name,
                                           net::CookieAccessResult result);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartitionAndVersionAndConsent(
      const login::GaiaContext& context,
      const std::string& partition_name,
      const std::string* platform_version,
      const bool* collect_stats_consent);

  // Sends request to reload Gaia. If `force_reload` is true, request
  // will be sent in any case, otherwise it will be sent only when Gaia is
  // not loading right now.
  void ReloadGaia(bool force_reload);

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void InitAfterJavascriptAllowed() override;

  // WebUI message handlers.
  void HandleWebviewLoadAborted(int error_code);

  // Handles Authenticator's 'completeAuthentication' event and gathers all the
  // data into `OnlineSigninArtifacts` and fetches cookies.
  void HandleCompleteAuthenticationEvent(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      const base::Value::List& scraped_saml_passwords_value,
      bool using_saml,
      const base::Value::List& services_list,
      bool services_provided,
      const base::Value::Dict& password_attributes,
      const base::Value::Dict& sync_trusted_vault_keys);

  // Intermediate step when cookies are received. The cookies are added into
  // their final location within `OnlineSigninArtifacts` and then passed to
  // `CompleteAuthentication`.
  void CompleteAuthWithCookies(ash::login::OnlineSigninArtifacts artifacts,
                               login::GaiaCookiesData gaia_cookies);

  // Final step of the `completeAuthentication` flow.
  // TODO(b/292242156) - Move to OnlineAuthenticationScreen
  void CompleteAuthentication(ash::login::OnlineSigninArtifacts artifacts);

  // Utility method gathering all the metrics that are being recorded when Gaia
  // sends 'completeAuthentication'.
  void RecordCompleteAuthenticationMetrics(
      const ash::login::OnlineSigninArtifacts& artifacts);

  void HandleLaunchSAMLPublicSession(const std::string& email);

  // Handles SAML/GAIA login flow metrics
  // is_third_party_idp == false means GAIA-based authentication
  void HandleUsingSAMLAPI(bool is_third_party_idp);
  void HandleRecordSAMLProvider(const std::string& x509certificate);
  void HandleSamlChallengeMachineKey(const std::string& callback_id,
                                     const std::string& url,
                                     const std::string& challenge);
  void HandleSamlChallengeMachineKeyResult(base::Value callback_id,
                                           base::Value::Dict result);

  void HandleGaiaUIReady();

  void HandleAuthenticatorLoaded();

  // Allows WebUI to control the login shelf's guest and apps buttons visibility
  // during OOBE.
  void HandleIsFirstSigninStep(bool is_first);

  // Called to notify whether the SAML sign-in is currently happening.
  void HandleSamlStateChanged(bool is_saml);
  // Called to deliver the result of the security token PIN request. Called with
  // an empty string when the request is canceled.
  void HandleSecurityTokenPinEntered(const std::string& user_input);
  void HandleOnFatalError(int error_code, const base::Value::Dict& params);

  // Called when the user is removed.
  void HandleUserRemoved(const std::string& email);

  // Called when password is entered for authentication during login.
  void HandlePasswordEntered();

  void HandleShowLoadingTimeoutError();

  // Called when Gaia sends us a "getDeviceId" message.
  void HandleGetDeviceId(const std::string& callback_id);

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(base::OnceClosure on_clear_callback);
  void OnCookiesCleared(base::OnceClosure on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Attempts login for test.
  void SubmitLoginFormForTest();

  // Updates the member variable and UMA histogram indicating whether the
  // Chrome Credentials Passing API was used during SAML login.
  void SetSAMLPrincipalsAPIUsed(bool is_third_party_idp, bool is_api_used);

  void RecordScrapedPasswordCount(int password_count);

  // True when client certificates were used during authentication. This is only
  // used for SmartCards and only when using SAML.
  bool ClientCertificatesWereUsed();

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. `force` is used to force the
  // extension reloading, if it has already been loaded.
  void LoadAuthenticator(bool force);

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);

  // HttpAuthDialog::Observer implementation:
  void HttpAuthDialogShown(content::WebContents* web_contents) override;
  void HttpAuthDialogCancelled(content::WebContents* web_contents) override;
  void HttpAuthDialogSupplied(content::WebContents* web_contents) override;

  // Returns true if current visible screen is the Gaia sign-in page.
  bool IsGaiaVisible();

  // Returns true if current visible screen is the error screen over
  // Gaia sign-in page.
  bool IsGaiaHiddenByError();

  // After proxy auth is cancelled or information has been supplied, this
  // function re-enables responding to network state notifications, and
  // reactivates the authentication flow autoreload functionality (if enabled by
  // policy).
  void OnProxyAuthDone();

  // Error screen hide callback which records error screen metrics and shows
  // GAIA.
  void OnErrorScreenHide();

  // Returns temporary unused device Id.
  std::string GetTemporaryDeviceId();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

  void OnCookieWaitTimeout();

  bool is_security_token_pin_dialog_running() const {
    return !security_token_pin_dialog_closed_callback_.is_null();
  }

  // Assigns new SamlChallengeKeyHandler object or an object for testing to
  // `saml_challenge_key_handler_`.
  void CreateSamlChallengeKeyHandler();

  // Current state of Gaia frame.
  FrameState frame_state_ = FRAME_STATE_UNKNOWN;

  // Latest Gaia frame error.
  net::Error frame_error_ = net::OK;

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

  // Makes untrusted authority certificates from device policy available for
  // client certificate discovery.
  std::unique_ptr<network::NSSTempCertsCacheChromeOS>
      untrusted_authority_certs_cache_;

  // The type of Gaia page to show.
  WizardContext::GaiaScreenMode screen_mode_ =
      WizardContext::GaiaScreenMode::kDefault;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<PublicSamlUrlFetcher> public_saml_url_fetcher_;

  // Used to store the Gaia reauth request token for Cryptohome recovery flow.
  std::string gaia_reauth_request_token_;

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

  bool hidden_ = true;

  // Used to record amount of time user needed for successful online login.
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;

  std::string signin_partition_name_;

  GaiaLoginVariant login_request_variant_ = GaiaLoginVariant::kUnknown;

  // Handler for `samlChallengeMachineKey` request.
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_;
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_for_test_;

  std::unique_ptr<GaiaCookieRetriever> gaia_cookie_retriever_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  const raw_ptr<ErrorScreen, DanglingUntriaged> error_screen_;

  NetworkStateInformer::State last_network_state_ =
      NetworkStateInformer::UNKNOWN;

  base::CancelableOnceCallback<void()> update_state_callback_;
  base::CancelableOnceCallback<void()> connecting_callback_;

  // Once Lacros is shipped, this will no longer be necessary.
  std::unique_ptr<HttpAuthDialog::ScopedEnabler> enable_ash_httpauth_;

  // Whether we're currently ignoring network state updates because a proxy auth
  // UI pending (or we're waiting for a grace period after the proxy auth UI is
  // finished for the network to switch into the ONLINE state).
  bool network_state_ignored_until_proxy_auth_ = false;

  // Used for pending GAIA reloads.
  NetworkError::ErrorReason gaia_reload_reason_ =
      NetworkError::ERROR_REASON_NONE;

  // If network has accidentally changed to the one that requires proxy
  // authentication, we will automatically reload gaia page that will bring
  // "Proxy authentication" dialog to the user. To prevent flakiness, we will do
  // it at most 3 times.
  int proxy_auth_dialog_reload_times_ = 3;

  // True if we need to reload gaia page to bring back "Proxy authentication"
  // dialog.
  bool proxy_auth_dialog_need_reload_ = false;

  bool is_offline_timeout_for_test_set_ = false;

  // Timeout to delay first notification about offline state for a
  // current network.
  base::TimeDelta offline_timeout_ = base::Seconds(1);

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  bool is_gaia_password_required_ = false;

  ash::AuthenticationFlowAutoReloadManager auth_flow_auto_reload_manager_;

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_SCREEN_HANDLER_H_
