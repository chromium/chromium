// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class NetworkScreen;
}

namespace chromeos {

// Interface of network screen. Owned by NetworkScreen.
class NetworkScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"network-selection"};

  virtual ~NetworkScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::NetworkScreen* screen) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const std::u16string& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  // Enables or disables offline Demo Mode during Demo Mode network selection.
  virtual void SetOfflineDemoModeEnabled(bool enabled) = 0;
};

// WebUI implementation of NetworkScreenView. It is used to interact with
// the OOBE network selection screen.
class NetworkScreenHandler : public NetworkScreenView,
                             public BaseScreenHandler {
 public:
  using TView = NetworkScreenView;

  NetworkScreenHandler();

  NetworkScreenHandler(const NetworkScreenHandler&) = delete;
  NetworkScreenHandler& operator=(const NetworkScreenHandler&) = delete;

  ~NetworkScreenHandler() override;

 private:
  // NetworkScreenView:
  void Show() override;
  void Hide() override;
  void Bind(ash::NetworkScreen* screen) override;
  void Unbind() override;
  void ShowError(const std::u16string& message) override;
  void ClearErrors() override;
  void SetOfflineDemoModeEnabled(bool enabled) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
  void InitializeDeprecated() override;

  ash::NetworkScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::NetworkScreenHandler;
using ::chromeos::NetworkScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
