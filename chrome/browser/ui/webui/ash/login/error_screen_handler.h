// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between ErrorScreen and its actual
// representation. Owned by ErrorScreen.
class ErrorScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"error-message",
                                                       "ErrorMessageScreen"};

  virtual ~ErrorScreenView() = default;

  // Shows the contents of the screen.
  virtual void ShowScreenWithParam(bool is_closeable) = 0;

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

  // Sets current UI state of the screen.
  virtual void SetUIState(NetworkError::UIState ui_state) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<ErrorScreenView> AsWeakPtr() = 0;
};

// A class that handles the WebUI hooks in error screen.
class ErrorScreenHandler final : public BaseScreenHandler,
                                 public ErrorScreenView {
 public:
  using TView = ErrorScreenView;

  ErrorScreenHandler();

  ErrorScreenHandler(const ErrorScreenHandler&) = delete;
  ErrorScreenHandler& operator=(const ErrorScreenHandler&) = delete;

  ~ErrorScreenHandler() override;

 private:
  // ErrorScreenView:
  void ShowScreenWithParam(bool is_closeable) override;
  void ShowOobeScreen(OobeScreenId screen) override;
  void SetErrorStateCode(NetworkError::ErrorState error_state) override;
  void SetErrorStateNetwork(const std::string& network_name) override;
  void SetGuestSigninAllowed(bool value) override;
  void SetOfflineSigninAllowed(bool value) override;
  void SetShowConnectingIndicator(bool value) override;
  void SetUIState(NetworkError::UIState ui_state) override;
  base::WeakPtr<ErrorScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // WebUI message handlers.
  void HandleHideCaptivePortal();

  base::WeakPtrFactory<ErrorScreenHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ERROR_SCREEN_HANDLER_H_
