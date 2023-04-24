// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Interface for UI implementations of the AppLaunchSplashScreen.
class AppLaunchSplashScreenView {
 public:
  class Delegate {
   public:
    // Invoked when the configure network control is clicked.
    virtual void OnConfigureNetwork() {}

    // Invoked when the network config did prepare network and is closed.
    virtual void OnNetworkConfigFinished() {}

    // Invoked when network state is changed. `online` is true if the device
    // is connected to the Internet.
    virtual void OnNetworkStateChanged(bool online) {}
  };

  enum class AppLaunchState {
    kPreparingProfile,
    kPreparingNetwork,
    kInstallingApplication,
    kInstallingExtension,
    kWaitingAppWindow,
    kWaitingAppWindowInstallFailed,
    kNetworkWaitTimeout,
    kShowingNetworkConfigureUI,
  };

  inline constexpr static StaticOobeScreenId kScreenId{"app-launch-splash",
                                                       "AppLaunchSplashScreen"};

  virtual ~AppLaunchSplashScreenView() {}

  // Sets screen controller this view belongs to.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Shows the contents of the screen.
  virtual void Show(KioskAppManagerBase::App app_data) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Set the current app launch state.
  virtual void UpdateAppLaunchState(AppLaunchState state) = 0;

  // Sets whether configure network control is visible.
  virtual void ToggleNetworkConfig(bool visible) = 0;

  // Shows the network error and configure UI.
  virtual void ShowNetworkConfigureUI() = 0;

  // Show a notification bar with error message.
  virtual void ShowErrorMessage(KioskAppLaunchError::Error error) = 0;

  // Returns true if the default network has Internet access.
  virtual bool IsNetworkReady() = 0;

  // Continues app launch after error screen is shown.
  virtual void ContinueAppLaunch() = 0;

  // Tells the splash screen view that network is required.
  virtual void SetNetworkRequired() = 0;
};

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler
    : public BaseScreenHandler,
      public AppLaunchSplashScreenView,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = AppLaunchSplashScreenView;

  AppLaunchSplashScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreen* error_screen);

  AppLaunchSplashScreenHandler(const AppLaunchSplashScreenHandler&) = delete;
  AppLaunchSplashScreenHandler& operator=(const AppLaunchSplashScreenHandler&) =
      delete;

  ~AppLaunchSplashScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;

  // AppLaunchSplashScreenView implementation:
  void Show(KioskAppManagerBase::App app_data) override;
  void Hide() override;
  void ToggleNetworkConfig(bool visible) override;
  void UpdateAppLaunchState(AppLaunchState state) override;
  void SetDelegate(Delegate* controller) override;
  void ShowNetworkConfigureUI() override;
  void ShowErrorMessage(KioskAppLaunchError::Error error) override;
  bool IsNetworkReady() override;
  void ContinueAppLaunch() override;
  void SetNetworkRequired() override;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(AppLaunchState state);
  void HandleConfigureNetwork();
  void DoToggleNetworkConfig(bool visible);

  raw_ptr<Delegate, ExperimentalAsh> delegate_ = nullptr;
  bool is_shown_ = false;
  bool is_network_required_ = false;
  AppLaunchState state_ = AppLaunchState::kPreparingProfile;

  scoped_refptr<NetworkStateInformer> network_state_informer_;
  raw_ptr<ErrorScreen, ExperimentalAsh> error_screen_;

  // Whether network configure UI is being shown.
  bool network_config_shown_ = false;

  // If this has value it will be populated through ToggleNetworkConfig(value)
  // after screen is shown. Cleared after screen was shown.
  absl::optional<bool> toggle_network_config_on_show_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
