// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class ActiveDirectoryPasswordChangeScreen;
}

namespace chromeos {

// Interface for dependency injection between
// ActiveDirectoryPasswordChangeScreen and its WebUI representation.
class ActiveDirectoryPasswordChangeView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"ad-password-change"};

  virtual ~ActiveDirectoryPasswordChangeView() {}

  // Shows the contents of the screen.
  virtual void Show(const std::string& username, int error) = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::ActiveDirectoryPasswordChangeScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Shows sign-in error bubble.
  virtual void ShowSignInError(const std::string& error_text) = 0;
};

// A class that handles WebUI hooks in Active Directory password change screen.
class ActiveDirectoryPasswordChangeScreenHandler
    : public ActiveDirectoryPasswordChangeView,
      public BaseScreenHandler {
 public:
  using TView = ActiveDirectoryPasswordChangeView;

  explicit ActiveDirectoryPasswordChangeScreenHandler(
      JSCallsContainer* js_calls_container);

  ActiveDirectoryPasswordChangeScreenHandler(
      const ActiveDirectoryPasswordChangeScreenHandler&) = delete;
  ActiveDirectoryPasswordChangeScreenHandler& operator=(
      const ActiveDirectoryPasswordChangeScreenHandler&) = delete;

  ~ActiveDirectoryPasswordChangeScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // ActiveDirectoryPasswordChangeView:
  void Show(const std::string& username, int error) override;
  void Bind(ash::ActiveDirectoryPasswordChangeScreen* screen) override;
  void Unbind() override;
  void ShowSignInError(const std::string& error_text) override;

 private:
  // WebUI message handlers.
  void HandleComplete(const std::string& old_password,
                      const std::string& new_password);

  ash::ActiveDirectoryPasswordChangeScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ActiveDirectoryPasswordChangeScreenHandler;
using ::chromeos::ActiveDirectoryPasswordChangeView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
