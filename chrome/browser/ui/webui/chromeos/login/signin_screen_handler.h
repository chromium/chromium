// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui.h"
#include "net/base/net_errors.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/events/event_handler.h"

class AccountId;

namespace ash {
namespace mojom {
enum class TrayActionState;
}  // namespace mojom
}  // namespace ash

namespace base {
class ListValue;
}

namespace chromeos {

class CoreOobeView;
class ErrorScreensHistogramHelper;
class GaiaScreenHandler;
class UserContext;

// An interface for WebUILoginDisplay to call SigninScreenHandler.
class LoginDisplayWebUIHandler {
 public:
  virtual void ClearAndEnablePassword() = 0;
  virtual void OnPreferencesChanged() = 0;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;
  virtual void ShowAllowlistCheckFailedError() = 0;

 protected:
  virtual ~LoginDisplayWebUIHandler() {}
};

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // --------------- Sign in/out methods.
  // Sign in using username and password specified as a part of `user_context`.
  // Used for both known and new users.
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) = 0;

  // Returns true if sign in is in progress.
  virtual bool IsSigninInProgress() const = 0;

  // --------------- Shared with login display methods.
  // Notify the delegate when the sign-in UI is finished loading.
  virtual void OnSigninScreenReady() = 0;

  // Shows Enterprise Enrollment screen.
  virtual void ShowEnterpriseEnrollmentScreen() = 0;

  // Shows Reset screen.
  virtual void ShowKioskAutolaunchScreen() = 0;

  // Show wrong hwid screen.
  virtual void ShowWrongHWIDScreen() = 0;

  // --------------- Rest of the methods.
  // Cancels user adding.
  virtual void CancelUserAdding() = 0;

  // Let the delegate know about the handler it is supposed to be using.
  virtual void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) = 0;

  // Whether the allow new user setting has changed.
  virtual bool AllowNewUserChanged() const = 0;

  // Whether user sign in has completed.
  virtual bool IsUserSigninCompleted() const = 0;

 protected:
  virtual ~SigninScreenHandlerDelegate() {}
};

// A class that handles the WebUI hooks in sign-in screen in OobeUI and
// LoginDisplay.
class SigninScreenHandler
    : public BaseWebUIHandler,
      public LoginDisplayWebUIHandler,
      public content::NotificationObserver,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  SigninScreenHandler(
      JSCallsContainer* js_calls_container,
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreen* error_screen,
      CoreOobeView* core_oobe_view,
      GaiaScreenHandler* gaia_screen_handler);
  ~SigninScreenHandler() override;

  static std::string GetUserLastInputMethod(const std::string& username);

  // Shows the sign in screen.
  void Show(bool oobe_ui);

  // Sets delegate to be used by the handler. It is guaranteed that valid
  // delegate is set before Show() method will be called.
  void SetDelegate(SigninScreenHandlerDelegate* delegate);

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

  // Required Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // To avoid spurious error messages on flaky networks, the offline message is
  // only shown if the network is offline for a threshold number of seconds.
  // This method provides an ability to reduce the threshold to zero, allowing
  // the offline message to show instantaneously in tests. The threshold can
  // also be set to a high value to disable the offline message on slow
  // configurations like MSAN, where it otherwise triggers on every run.
  void SetOfflineTimeoutForTesting(base::TimeDelta offline_timeout);

  // Gets the keyboard remapped pref value for `pref_name` key. Returns true if
  // successful, otherwise returns false.
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name, int* value);

 private:
  // TODO (crbug.com/1168114): check if it makes sense anymore, as we're always
  // showing GAIA
  enum UIState {
    UI_STATE_UNKNOWN = 0,
    UI_STATE_GAIA_SIGNIN,
  };

  friend class GaiaScreenHandler;
  friend class ReportDnsCacheClearedOnUIThread;
  friend class LoginDisplayHostMojo;

  void ShowImpl();

  // Updates current UI of the signin screen according to `ui_state`
  // argument.
  void UpdateUIState(UIState ui_state);

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void SetupAndShowOfflineMessage(NetworkStateInformer::State state,
                                  NetworkError::ErrorReason reason);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);
  void ReloadGaia(bool force_reload);

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // LoginDisplayWebUIHandler implementation:
  void ClearAndEnablePassword() override;
  void OnPreferencesChanged() override;
  void ShowError(int login_attempts,
                 const std::string& error_text,
                 const std::string& help_link_text,
                 HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowAllowlistCheckFailedError() override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // WebUI message handlers.
  void HandleAuthenticateUser(const AccountId& account_id,
                              const std::string& password,
                              bool authenticated_by_pin);
  void HandleLaunchIncognito();
  void HandleLaunchSAMLPublicSession(const std::string& email);
  void HandleOfflineLogin(const base::ListValue* args);
  void HandleToggleEnrollmentScreen();
  void HandleToggleResetScreen();
  void HandleToggleKioskAutolaunchScreen();

  void HandleOpenInternetDetailDialog();
  void HandleLoginVisible(const std::string& source);
  void HandleLoginUIStateChanged(const std::string& source, bool active);
  void HandleLoginScreenUpdate();
  void HandleShowLoadingTimeoutError();
  void HandleNoPodFocused();
  void HandleHardlockPod(const std::string& user_id);
  void HandleLaunchKioskApp(const AccountId& app_account_id,
                            bool diagnostic_mode);
  void HandleLaunchArcKioskApp(const AccountId& app_account_id);

  // Implements user sign-in.
  void AuthenticateExistingUser(const AccountId& account_id,
                                const std::string& password,
                                bool authenticated_by_pin);

  // Returns true iff
  // (i)   log in is restricted to some user list,
  // (ii)  all users in the restricted list are present.
  bool AllAllowlistedUsersPresent();

  // Returns true if current visible screen is the Gaia sign-in page.
  bool IsGaiaVisible() const;

  // Returns true if current visible screen is the error screen over
  // Gaia sign-in page.
  bool IsGaiaHiddenByError() const;

  // Returns true if current screen is the error screen over signin
  // screen.
  bool IsSigninScreenHiddenByError() const;

  net::Error FrameError() const;


  // After proxy auth information has been supplied, this function re-enables
  // responding to network state notifications.
  void ReenableNetworkStateUpdatesAfterProxyAuth();

  // Current UI state of the signin screen.
  UIState ui_state_ = UI_STATE_UNKNOWN;

  // A delegate that glues this handler with backend LoginDisplay.
  SigninScreenHandlerDelegate* delegate_ = nullptr;

  // Whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Keeps whether screen should be shown for OOBE.
  bool oobe_ui_ = false;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Set to true once `LOGIN_WEBUI_VISIBLE` notification is observed.
  bool webui_visible_ = false;
  bool preferences_changed_delayed_ = false;

  ErrorScreen* error_screen_ = nullptr;
  CoreOobeView* core_oobe_view_ = nullptr;

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
  int proxy_auth_dialog_reload_times_;

  // True if we need to reload gaia page to bring back "Proxy authentication"
  // dialog.
  bool proxy_auth_dialog_need_reload_ = false;

  // Non-owning ptr.
  // TODO(antrim@): remove this dependency.
  GaiaScreenHandler* gaia_screen_handler_ = nullptr;

  // Input Method Engine state used at signin screen.
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  bool is_offline_timeout_for_test_set_ = false;
  base::TimeDelta offline_timeout_for_test_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  // TODO (crbug.com/1168114): Only needed for GetKeyboardRemappedPrefValue that
  // should be migrated.
  std::unique_ptr<AccountId> focused_pod_account_id_;

  base::WeakPtrFactory<SigninScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::SigninScreenHandler;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
