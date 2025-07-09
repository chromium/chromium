// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

namespace ash {

// Interface for UI implementations of the AppLaunchSplashScreen.
class AppLaunchSplashScreenView {
 public:
  enum class AppLaunchState {
    kPreparingProfile,
    kPreparingNetwork,
    kInstallingApplication,
    kInstallingExtension,
    kWaitingAppWindow,
    kNetworkWaitTimeout,
    kShowingNetworkConfigureUI,
    kChromeAppDeprecated,
    kIsolatedAppNotAllowed
  };

  inline constexpr static StaticOobeScreenId kScreenId{"app-launch-splash",
                                                       "AppLaunchSplashScreen"};

  virtual ~AppLaunchSplashScreenView() = default;

  // Shows the screen after it's populated with the given `data`.
  virtual void Show(base::Value::Dict data) = 0;

  // Sets the current app launch state.
  virtual void UpdateAppLaunchText(AppLaunchState state) = 0;

  virtual void HideThrobber() = 0;

  // Sets the contents of the screen with the given `data`.
  virtual void SetAppData(base::Value::Dict data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<AppLaunchSplashScreenView> AsWeakPtr() = 0;
};

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler : public BaseScreenHandler,
                                     public AppLaunchSplashScreenView {
 public:
  using TView = AppLaunchSplashScreenView;

  AppLaunchSplashScreenHandler();

  AppLaunchSplashScreenHandler(const AppLaunchSplashScreenHandler&) = delete;
  AppLaunchSplashScreenHandler& operator=(const AppLaunchSplashScreenHandler&) =
      delete;

  ~AppLaunchSplashScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AppLaunchSplashScreenView implementation:
  void Show(base::Value::Dict data) override;
  void UpdateAppLaunchText(AppLaunchState state) override;
  void HideThrobber() override;
  void SetAppData(base::Value::Dict data) override;

  // Gets a WeakPtr to the instance.
  base::WeakPtr<AppLaunchSplashScreenView> AsWeakPtr() override;

 private:
  int GetProgressMessageFromState(AppLaunchState state);
  base::WeakPtrFactory<AppLaunchSplashScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
