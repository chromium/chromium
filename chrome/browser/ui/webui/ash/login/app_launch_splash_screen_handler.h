// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include <set>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

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
  };

  // The data struct needed to populate this view.
  struct Data {
    Data(std::string_view name, gfx::ImageSkia icon, const GURL& url);
    Data(const Data&) = delete;
    Data(Data&&);
    Data& operator=(const Data&) = delete;
    Data& operator=(Data&&);
    ~Data();

    // The name of the app.
    std::string name;
    // The icon of the app.
    gfx::ImageSkia icon;
    // The URL of the app.
    GURL url;
  };

  enum class AppLaunchState {
    kPreparingProfile,
    kPreparingNetwork,
    kInstallingApplication,
    kInstallingExtension,
    kWaitingAppWindow,
    kNetworkWaitTimeout,
    kShowingNetworkConfigureUI,
  };

  inline constexpr static StaticOobeScreenId kScreenId{"app-launch-splash",
                                                       "AppLaunchSplashScreen"};

  virtual ~AppLaunchSplashScreenView() = default;

  // Sets screen controller this view belongs to.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Shows or updates the contents of the screen with the given `data`.
  virtual void Show(Data data) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Set the current app launch state.
  virtual void UpdateAppLaunchState(AppLaunchState state) = 0;

  // Sets whether configure network control is visible.
  virtual void ToggleNetworkConfig(bool visible) = 0;

  // Shows the network error and configure UI.
  virtual void ShowNetworkConfigureUI(NetworkStateInformer::State state,
                                      const std::string& network_name) = 0;

  // Show a notification bar with error message.
  virtual void ShowErrorMessage(KioskAppLaunchError::Error error) = 0;

  // Continues app launch after error screen is shown.
  virtual void ContinueAppLaunch() = 0;
};

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler : public BaseScreenHandler,
                                     public AppLaunchSplashScreenView {
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
  void Show(Data data) override;
  void Hide() override;
  void ToggleNetworkConfig(bool visible) override;
  void UpdateAppLaunchState(AppLaunchState state) override;
  void SetDelegate(Delegate* controller) override;
  void ShowNetworkConfigureUI(NetworkStateInformer::State state,
                              const std::string& network_name) override;
  void ShowErrorMessage(KioskAppLaunchError::Error error) override;
  void ContinueAppLaunch() override;

 private:
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(AppLaunchState state);
  void HandleConfigureNetwork();
  void DoToggleNetworkConfig(bool visible);

  raw_ptr<Delegate, ExperimentalAsh> delegate_ = nullptr;
  bool is_shown_ = false;
  AppLaunchState state_ = AppLaunchState::kPreparingProfile;

  raw_ptr<ErrorScreen, DanglingUntriaged | ExperimentalAsh> error_screen_;

  // Whether network configure UI is being shown.
  bool network_config_shown_ = false;

  // If this has value it will be populated through ToggleNetworkConfig(value)
  // after screen is shown. Cleared after screen was shown.
  absl::optional<bool> toggle_network_config_on_show_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
