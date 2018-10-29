// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DEMO_SETUP_SCREEN_HANDLER_H_

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class DemoSetupScreen;

// WebUI implementation of DemoSetupScreenView. It controlls UI, receives UI
// events and notifies the Delegate.
class DemoSetupScreenHandler : public BaseScreenHandler,
                               public DemoSetupScreenView {
 public:
  DemoSetupScreenHandler();
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
