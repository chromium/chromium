// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

namespace chromeos {

// Interface for UI implementations of the AppLaunchSplashScreen.
class AppLaunchSplashScreenView {
 public:
  class Delegate {
   public:
    // Invoked when the configure network control is clicked.
    virtual void OnConfigureNetwork() {}

    // Invoked when the app launch bailout shortcut key is pressed.
    virtual void OnCancelAppLaunch() {}

    // Invoked when the network config shortcut key is pressed.
    virtual void OnNetworkConfigRequested() {}

    // Invoked when the network config did prepare network and is closed.
    virtual void OnNetworkConfigFinished() {}

    // Invoked when network state is changed. |online| is true if the device
    // is connected to the Internet.
    virtual void OnNetworkStateChanged(bool online) {}

    // Invoked when the splash screen view is being deleted.
    virtual void OnDeletingSplashScreenView() {}

    // Returns the data needed to be displayed on the splash screen.
    virtual KioskAppManagerBase::App GetAppData() = 0;
  };

  enum AppLaunchState {
    APP_LAUNCH_STATE_PREPARING_NETWORK,
    APP_LAUNCH_STATE_INSTALLING_APPLICATION,
    APP_LAUNCH_STATE_WAITING_APP_WINDOW,
    APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT,
    APP_LAUNCH_STATE_SHOWING_NETWORK_CONFIGURE_UI,
  };

  constexpr static StaticOobeScreenId kScreenId{"app-launch-splash"};

  virtual ~AppLaunchSplashScreenView() {}

  // Sets screen controller this view belongs to.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Set the current app launch state.
  virtual void UpdateAppLaunchState(AppLaunchState state) = 0;

  // Sets whether configure network control is visible.
  virtual void ToggleNetworkConfig(bool visible) = 0;

  // Shows the network error and configure UI.
  virtual void ShowNetworkConfigureUI() = 0;

  // Returns true if the default network has Internet access.
  virtual bool IsNetworkReady() = 0;
};

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler
    : public BaseScreenHandler,
      public AppLaunchSplashScreenView,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = AppLaunchSplashScreenView;

  AppLaunchSplashScreenHandler(
      JSCallsContainer* js_calls_container,
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreen* error_screen);
  ~AppLaunchSplashScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // AppLaunchSplashScreenView implementation:
  void Show() override;
  void Hide() override;
  void ToggleNetworkConfig(bool visible) override;
  void UpdateAppLaunchState(AppLaunchState state) override;
  void SetDelegate(Delegate* controller) override;
  void ShowNetworkConfigureUI() override;
  bool IsNetworkReady() override;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void OnNetworkReady() override;
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  void PopulateAppInfo(base::DictionaryValue* out_info);
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(AppLaunchState state);
  void HandleConfigureNetwork();
  void HandleCancelAppLaunch();
  void HandleContinueAppLaunch();
  void HandleNetworkConfigRequested();

  Delegate* delegate_ = nullptr;
  bool show_on_init_ = false;
  AppLaunchState state_ = APP_LAUNCH_STATE_PREPARING_NETWORK;

  scoped_refptr<NetworkStateInformer> network_state_informer_;
  ErrorScreen* error_screen_;

  // True if we are online.
  bool online_state_ = false;

  // True if we have network config screen was already shown before.
  bool network_config_done_ = false;

  // True if we have manually requested network config screen.
  bool network_config_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchSplashScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
