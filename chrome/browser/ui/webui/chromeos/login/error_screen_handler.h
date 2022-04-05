// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class ErrorScreen;
}

namespace chromeos {

// Interface for dependency injection between ErrorScreen and its actual
// representation. Owned by ErrorScreen.
class ErrorScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"error-message"};

  virtual ~ErrorScreenView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::ErrorScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Switches to `screen`.
  virtual void ShowOobeScreen(OobeScreenId screen) = 0;

  // Sets current error state of the screen.
  virtual void SetErrorStateCode(NetworkError::ErrorState error_state) = 0;

  // Sets current error network state of the screen.
  virtual void SetErrorStateNetwork(const std::string& network_name) = 0;

  // Is guest signin allowed?
  virtual void SetGuestSigninAllowed(bool value) = 0;

  // Is offline signin allowed?
  virtual void SetOfflineSigninAllowed(bool value) = 0;

  // Updates visibility of the label indicating we're reconnecting.
  virtual void SetShowConnectingIndicator(bool value) = 0;

  // Makes error persistent (e.g. non-closable).
  virtual void SetIsPersistentError(bool is_persistent) = 0;

  // Sets current UI state of the screen.
  virtual void SetUIState(NetworkError::UIState ui_state) = 0;

  // Reloads gaia.
  virtual void OnReloadGaiaClicked() = 0;
};

// A class that handles the WebUI hooks in error screen.
class ErrorScreenHandler : public BaseScreenHandler, public ErrorScreenView {
 public:
  using TView = ErrorScreenView;

  ErrorScreenHandler();

  ErrorScreenHandler(const ErrorScreenHandler&) = delete;
  ErrorScreenHandler& operator=(const ErrorScreenHandler&) = delete;

  ~ErrorScreenHandler() override;

 private:
  // ErrorScreenView:
  void Show() override;
  void Hide() override;
  void Bind(ash::ErrorScreen* screen) override;
  void Unbind() override;
  void ShowOobeScreen(OobeScreenId screen) override;
  void SetErrorStateCode(NetworkError::ErrorState error_state) override;
  void SetErrorStateNetwork(const std::string& network_name) override;
  void SetGuestSigninAllowed(bool value) override;
  void SetOfflineSigninAllowed(bool value) override;
  void SetShowConnectingIndicator(bool value) override;
  void SetIsPersistentError(bool is_persistent) override;
  void SetUIState(NetworkError::UIState ui_state) override;
  void OnReloadGaiaClicked() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

  // WebUI message handlers.
  void HandleHideCaptivePortal();

  // Non-owning ptr.
  ash::ErrorScreen* screen_ = nullptr;

  // Should the screen be shown right after initialization?
  bool show_on_init_ = false;

  // Whether the error screen is currently shown.
  bool showing_ = false;

  base::WeakPtrFactory<ErrorScreenHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ErrorScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
