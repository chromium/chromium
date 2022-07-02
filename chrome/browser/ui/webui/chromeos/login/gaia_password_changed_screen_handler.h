// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class GaiaPasswordChangedScreen;
}

namespace chromeos {

// Interface for dependency injection between GaiaPasswordChangedScreen and its
// WebUI representation.
class GaiaPasswordChangedView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"gaia-password-changed"};

  virtual ~GaiaPasswordChangedView() {}

  // Shows the contents of the screen.
  virtual void Show(const std::string& email, bool has_error) = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::GaiaPasswordChangedScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

class GaiaPasswordChangedScreenHandler : public GaiaPasswordChangedView,
                                         public BaseScreenHandler {
 public:
  using TView = GaiaPasswordChangedView;

  GaiaPasswordChangedScreenHandler();
  GaiaPasswordChangedScreenHandler(const GaiaPasswordChangedScreenHandler&) =
      delete;
  GaiaPasswordChangedScreenHandler& operator=(
      const GaiaPasswordChangedScreenHandler&) = delete;
  ~GaiaPasswordChangedScreenHandler() override;

 private:
  void Show(const std::string& email, bool has_error) override;
  void Bind(ash::GaiaPasswordChangedScreen* screen) override;
  void Unbind() override;

  void HandleMigrateUserData(const std::string& old_password);

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

  ash::GaiaPasswordChangedScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::GaiaPasswordChangedScreenHandler;
using ::chromeos::GaiaPasswordChangedView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_
