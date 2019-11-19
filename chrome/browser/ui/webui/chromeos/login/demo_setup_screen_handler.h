// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class DemoSetupScreen;

// Interface of the demo mode setup screen view.
class DemoSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"demo-setup"};

  virtual ~DemoSetupScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets view and screen.
  virtual void Bind(DemoSetupScreen* screen) = 0;

  // Handles successful setup.
  virtual void OnSetupSucceeded() = 0;

  // Handles setup failure.
  virtual void OnSetupFailed(
      const DemoSetupController::DemoSetupError& error) = 0;
};

// WebUI implementation of DemoSetupScreenView. It controlls UI, receives UI
// events and notifies the Delegate.
class DemoSetupScreenHandler : public BaseScreenHandler,
                               public DemoSetupScreenView {
 public:
  using TView = DemoSetupScreenView;

  explicit DemoSetupScreenHandler(JSCallsContainer* js_calls_container);
  ~DemoSetupScreenHandler() override;

  // DemoSetupScreenView:
  void Show() override;
  void Hide() override;
  void Bind(DemoSetupScreen* screen) override;
  void OnSetupFailed(const DemoSetupController::DemoSetupError& error) override;
  void OnSetupSucceeded() override;

  // BaseScreenHandler:
  void Initialize() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  DemoSetupScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
