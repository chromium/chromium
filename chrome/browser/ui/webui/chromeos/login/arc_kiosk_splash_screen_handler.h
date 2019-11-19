// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_KIOSK_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_KIOSK_SPLASH_SCREEN_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ArcKioskController;

// Interface for UI implementations of the ArcKioskSplashScreen.
class ArcKioskSplashScreenView {
 public:
  enum class ArcKioskState {
    STARTING_SESSION,
    WAITING_APP_LAUNCH,
    WAITING_APP_WINDOW,
  };

  constexpr static StaticOobeScreenId kScreenId{"arc-kiosk-splash"};

  ArcKioskSplashScreenView() = default;

  virtual ~ArcKioskSplashScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Set the current ARC kiosk state.
  virtual void UpdateArcKioskState(ArcKioskState state) = 0;

  // Sets screen this view belongs to.
  virtual void SetDelegate(ArcKioskController* controller) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcKioskSplashScreenView);
};

// A class that handles the WebUI hooks for the ARC kiosk splash screen.
class ArcKioskSplashScreenHandler : public BaseScreenHandler,
                                    public ArcKioskSplashScreenView {
 public:
  using TView = ArcKioskSplashScreenView;

  explicit ArcKioskSplashScreenHandler(JSCallsContainer* js_calls_container);
  ~ArcKioskSplashScreenHandler() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // ArcKioskSplashScreenView implementation:
  void Show() override;
  void UpdateArcKioskState(ArcKioskState state) override;
  void SetDelegate(ArcKioskController* controller) override;

  void PopulateAppInfo(base::DictionaryValue* out_info);
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(ArcKioskState state);
  void HandleCancelArcKioskLaunch();

  ArcKioskController* controller_ = nullptr;
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskSplashScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_KIOSK_SPLASH_SCREEN_HANDLER_H_
