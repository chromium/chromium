// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class PinSetupScreen;
}

namespace chromeos {

// Interface for dependency injection between PinSetupScreen and its
// WebUI representation.
class PinSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"pin-setup"};

  virtual ~PinSetupScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(ash::PinSetupScreen* screen) = 0;

  // Shows the contents of the screen, using |token| to access QuickUnlock API.
  virtual void Show(const std::string& token, bool is_child_account) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  virtual void SetLoginSupportAvailable(bool available) = 0;
};

// The sole implementation of the PinSetupScreenView, using WebUI.
class PinSetupScreenHandler : public BaseScreenHandler,
                              public PinSetupScreenView {
 public:
  using TView = PinSetupScreenView;

  PinSetupScreenHandler();

  PinSetupScreenHandler(const PinSetupScreenHandler&) = delete;
  PinSetupScreenHandler& operator=(const PinSetupScreenHandler&) = delete;

  ~PinSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
  void RegisterMessages() override;

  // PinSetupScreenView:
  void Bind(ash::PinSetupScreen* screen) override;
  void Hide() override;
  void InitializeDeprecated() override;
  void Show(const std::string& token, bool is_child_account) override;
  void SetLoginSupportAvailable(bool available) override;

 private:
  ash::PinSetupScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::PinSetupScreenHandler;
using ::chromeos::PinSetupScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
