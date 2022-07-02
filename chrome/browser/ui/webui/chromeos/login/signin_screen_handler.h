// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "ash/components/proximity_auth/screenlock_bridge.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui.h"
#include "net/base/net_errors.h"
#include "ui/events/event_handler.h"

namespace ash {
class LoginDisplayHostMojo;

namespace mojom {
enum class TrayActionState;
}  // namespace mojom
}  // namespace ash

namespace chromeos {

class GaiaScreenHandler;

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // --------------- Sign in/out methods.
  // Returns true if sign in is in progress.
  virtual bool IsSigninInProgress() const = 0;

  // --------------- Rest of the methods.

  // Whether user sign in has completed.
  virtual bool IsUserSigninCompleted() const = 0;

 protected:
  virtual ~SigninScreenHandlerDelegate() = default;
};

// A class that handles the WebUI hooks in sign-in screen in OobeUI and
// LoginDisplay.
class SigninScreenHandler
    : public BaseWebUIHandler,
      public content::NotificationObserver,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  SigninScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreen* error_screen,
      GaiaScreenHandler* gaia_screen_handler);

  SigninScreenHandler(const SigninScreenHandler&) = delete;
  SigninScreenHandler& operator=(const SigninScreenHandler&) = delete;

  ~SigninScreenHandler() override;

  // Shows the sign in screen.
  void Show();

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

 private:
  friend class GaiaScreenHandler;
  friend class ash::LoginDisplayHostMojo;
  friend class ReportDnsCacheClearedOnUIThread;

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);
  void ReloadGaia(bool force_reload);

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // WebUI message handlers.
  void HandleShowLoadingTimeoutError();

  // Returns true if current visible screen is the Gaia sign-in page.
  bool IsGaiaVisible();

  // Returns true if current visible screen is the error screen over
  // Gaia sign-in page.
  bool IsGaiaHiddenByError();

  net::Error FrameError() const;


  // After proxy auth information has been supplied, this function re-enables
  // responding to network state notifications.
  void ReenableNetworkStateUpdatesAfterProxyAuth();

  // Error screen hide callback which records error screen metrics and shows
  // GAIA.
  void OnErrorScreenHide();

  // A delegate that glues this handler with backend LoginDisplay.
  SigninScreenHandlerDelegate* delegate_ = nullptr;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  ErrorScreen* error_screen_ = nullptr;

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

  bool is_offline_timeout_for_test_set_ = false;
  base::TimeDelta offline_timeout_for_test_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::WeakPtrFactory<SigninScreenHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::SigninScreenHandler;
using ::chromeos::SigninScreenHandlerDelegate;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
