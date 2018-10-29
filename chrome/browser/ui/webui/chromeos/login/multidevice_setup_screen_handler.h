// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class MultiDeviceSetupScreen;

// Concrete MultiDeviceSetupScreenView WebUI-based implementation.
class MultiDeviceSetupScreenHandler : public BaseScreenHandler,
                                      public MultiDeviceSetupScreenView {
 public:
  MultiDeviceSetupScreenHandler();
  ~MultiDeviceSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // MultiDeviceSetupScreenView:
  void Bind(MultiDeviceSetupScreen* screen) override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
