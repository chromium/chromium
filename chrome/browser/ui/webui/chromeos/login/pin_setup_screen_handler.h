// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class PinSetupScreen;

// Interface for dependency injection between PinSetupScreen and its
// WebUI representation.
class PinSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"pin-setup"};

  virtual ~PinSetupScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(PinSetupScreen* screen) = 0;

  // Shows the contents of the screen, using |token| to access QuickUnlock API.
  virtual void Show(const std::string& token) = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  virtual void SetLoginSupportAvailable(bool available) = 0;
};

// The sole implementation of the PinSetupScreenView, using WebUI.
class PinSetupScreenHandler : public BaseScreenHandler,
                              public PinSetupScreenView {
 public:
  using TView = PinSetupScreenView;

  explicit PinSetupScreenHandler(JSCallsContainer* js_calls_container);
  ~PinSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void RegisterMessages() override;

  // PinSetupScreenView:
  void Bind(PinSetupScreen* screen) override;
  void Hide() override;
  void Initialize() override;
  void Show(const std::string& token) override;
  void SetLoginSupportAvailable(bool available) override;

 private:
  PinSetupScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PinSetupScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
