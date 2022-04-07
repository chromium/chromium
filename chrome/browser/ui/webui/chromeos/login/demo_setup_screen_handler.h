// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class DemoSetupScreen;
}

namespace chromeos {

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
  virtual void Bind(ash::DemoSetupScreen* screen) = 0;

  // Updates current setup step.
  virtual void SetCurrentSetupStep(
      DemoSetupController::DemoSetupStep current_step) = 0;

  // Handles successful setup.
  virtual void OnSetupSucceeded() = 0;

  // Handles setup failure.
  virtual void OnSetupFailed(
      const DemoSetupController::DemoSetupError& error) = 0;
};

// WebUI implementation of DemoSetupScreenView. It controls UI, receives UI
// events and notifies the Delegate.
class DemoSetupScreenHandler : public BaseScreenHandler,
                               public DemoSetupScreenView {
 public:
  using TView = DemoSetupScreenView;

  DemoSetupScreenHandler();

  DemoSetupScreenHandler(const DemoSetupScreenHandler&) = delete;
  DemoSetupScreenHandler& operator=(const DemoSetupScreenHandler&) = delete;

  ~DemoSetupScreenHandler() override;

  // DemoSetupScreenView:
  void Show() override;
  void Hide() override;
  void Bind(ash::DemoSetupScreen* screen) override;
  void SetCurrentSetupStep(
      DemoSetupController::DemoSetupStep current_step) override;
  void OnSetupFailed(const DemoSetupController::DemoSetupError& error) override;
  void OnSetupSucceeded() override;

  // BaseScreenHandler:
  void InitializeDeprecated() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // BaseWebUIHandler:
  void GetAdditionalParameters(base::Value::Dict* parameters) override;

 private:
  ash::DemoSetupScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::DemoSetupScreenHandler;
using ::chromeos::DemoSetupScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
