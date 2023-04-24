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
#include "chrome/browser/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/online_login_helper.h"
#include "chrome/browser/ui/webui/ash/login/saml_challenge_key_handler.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
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

class GaiaView : public base::SupportsWeakPtr<GaiaView> {
 public:
  enum class GaiaPath {
    kDefault,
    kChildSignup,
    kChildSignin,
    kReauth,
  };

  enum class GaiaLoginVariant {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    kUnknown = 0,
    kOobe = 1,
    kAddUser = 2,
    kOnlineSignin = 3,
    kMaxValue = kOnlineSignin
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
  // Sets Gaia path for sign-in, child sign-in or child sign-up.
  virtual void SetGaiaPath(GaiaPath gaia_path) = 0;
  // Show error UI at the end of GAIA flow when user is not allowlisted.
  virtual void ShowAllowlistCheckFailedError() = 0;
  // Reloads authenticator.
  virtual void ReloadGaiaAuthenticator() = 0;
  // Sets reauth request token in the URL, in order to get reauth proof token
  // for recovery.
  virtual void SetReauthRequestToken(
      const std::string& reauth_request_token) = 0;

  // Show sign-in screen for the given credentials. `services` is a list of
  // services returned by userInfo call as JSON array. Should be an empty array
  // for a regular user: "[]".
  virtual void ShowSigninScreenForTest(const std::string& username,
                                       const std::string& password,
                                       const std::string& services) = 0;
  // Reset authenticator.
  virtual void Reset() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler
    : public BaseScreenHandler,
      public GaiaView,
      public chromeos::SecurityTokenPinDialogHost,
      public content::NotificationObserver,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = GaiaView;

  // The possible modes that the Gaia signin screen can be in.
  enum GaiaScreenMode {
    // Default Gaia authentication will be used.
    GAIA_SCREEN_MODE_DEFAULT = 0,

    // SAML authentication will be used by default.
    GAIA_SCREEN_MODE_SAML_REDIRECT = 1,
  };

  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR
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
  void SetGaiaPath(GaiaPath gaia_path) override;
  void ShowAllowlistCheckFailedError() override;
  void ReloadGaiaAuthenticator() override;
  void SetReauthRequestToken(const std::string& reauth_request_token) override;

  void ShowSigninScreenForTest(const std::string& username,
                               const std::string& password,
                               const std::string& services) override;
  void Reset() override;

  // SecurityTokenPinDialogHost:
  void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      chromeos::security_token_pin::CodeType code_type,
      bool enable_user_input,
      chromeos::security_token_pin::ErrorLabel error_label,
      int attempts_left,
      const absl::optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) override;
  void CloseSecurityTokenPinDialog() override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

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
  void HandleCompleteAuthentication(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      const base::Value::List& scraped_saml_passwords_value,
      bool using_saml,
      const base::Value::List& services_list,
      bool services_provided,
      const base::Value::Dict& password_attributes,
      const base::Value::Dict& sync_trusted_vault_keys);
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);
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

  void HandleIdentifierEntered(const std::string& account_identifier);

  void HandleAuthExtensionLoaded();

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

  // Really handles the complete login message.
  void DoCompleteLogin(const std::string& gaia_id,
                       const std::string& typed_email,
                       const std::string& password,
                       bool using_saml);

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
  bool IsSamlUserPasswordless();

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. `force` is used to force the
  // extension reloading, if it has already been loaded.
  void LoadAuthExtension(bool force);

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns true if current visible screen is the Gaia sign-in page.
  bool IsGaiaVisible();

  // Returns true if current visible screen is the error screen over
  // Gaia sign-in page.
  bool IsGaiaHiddenByError();

  // After proxy auth information has been supplied, this function re-enables
  // responding to network state notifications.
  void ReenableNetworkStateUpdatesAfterProxyAuth();

  // Error screen hide callback which records error screen metrics and shows
  // GAIA.
  void OnErrorScreenHide();

  // Returns temporary unused device Id.
  std::string GetTemporaryDeviceId();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

  // Returns user canonical e-mail. Finds already used account alias, if
  // user has already signed in.
  AccountId GetAccountId(const std::string& authenticated_email,
                         const std::string& id,
                         const AccountType& account_type) const;

  void OnCookieWaitTimeout();

  bool is_security_token_pin_dialog_running() const {
    return !security_token_pin_dialog_closed_callback_.is_null();
  }

  // Assigns new SamlChallengeKeyHandler object or an object for testing to
  // `saml_challenge_key_handler_`.
  void CreateSamlChallengeKeyHandler();

  void SAMLConfirmPassword(::login::StringList scraped_saml_passwords,
                           std::unique_ptr<UserContext> user_context);

  bool MaybeTriggerEnrollmentNudge(const std::string& user_email);
  void CheckIfAllowlisted(const std::string& user_email);

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
  GaiaScreenMode screen_mode_ = GAIA_SCREEN_MODE_DEFAULT;

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

  GaiaPath gaia_path_ = GaiaPath::kDefault;

  bool hidden_ = true;

  // Used to record amount of time user needed for successful online login.
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;

  std::string signin_partition_name_;

  GaiaLoginVariant login_request_variant_ = GaiaLoginVariant::kUnknown;

  // Handler for `samlChallengeMachineKey` request.
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_;
  std::unique_ptr<SamlChallengeKeyHandler> saml_challenge_key_handler_for_test_;

  std::unique_ptr<OnlineLoginHelper> online_login_helper_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  const base::raw_ptr<ErrorScreen, DanglingUntriaged> error_screen_;

  NetworkStateInformer::State last_network_state_ =
      NetworkStateInformer::UNKNOWN;

  base::CancelableOnceCallback<void()> update_state_callback_;
  base::CancelableOnceCallback<void()> connecting_callback_;

  content::NotificationRegistrar registrar_;

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

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_SCREEN_HANDLER_H_
