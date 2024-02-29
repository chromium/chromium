// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface of the demo mode setup screen view.
class DemoSetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"demo-setup",
                                                       "DemoSetupScreen"};

  virtual ~DemoSetupScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Updates current setup step.
  virtual void SetCurrentSetupStep(
      DemoSetupController::DemoSetupStep current_step) = 0;

  // Handles successful setup.
  virtual void OnSetupSucceeded() = 0;

  // Handles setup failure.
  virtual void OnSetupFailed(
      const DemoSetupController::DemoSetupError& error) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<DemoSetupScreenView> AsWeakPtr() = 0;
};

// WebUI implementation of DemoSetupScreenView. It controls UI, receives UI
// events and notifies the Delegate.
class DemoSetupScreenHandler final : public BaseScreenHandler,
                                     public DemoSetupScreenView {
 public:
  using TView = DemoSetupScreenView;

  DemoSetupScreenHandler();

  DemoSetupScreenHandler(const DemoSetupScreenHandler&) = delete;
  DemoSetupScreenHandler& operator=(const DemoSetupScreenHandler&) = delete;

  ~DemoSetupScreenHandler() override;

  // DemoSetupScreenView:
  void Show() override;
  void SetCurrentSetupStep(
      DemoSetupController::DemoSetupStep current_step) override;
  void OnSetupFailed(const DemoSetupController::DemoSetupError& error) override;
  void OnSetupSucceeded() override;
  base::WeakPtr<DemoSetupScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // BaseWebUIHandler:
  void GetAdditionalParameters(base::Value::Dict* parameters) override;

 private:
  base::WeakPtrFactory<DemoSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
