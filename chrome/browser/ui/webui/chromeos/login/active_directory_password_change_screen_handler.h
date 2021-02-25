// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class ActiveDirectoryPasswordChangeScreen;

// Interface for dependency injection between
// ActiveDirectoryPasswordChangeScreen and its WebUI representation.
class ActiveDirectoryPasswordChangeView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"ad-password-change"};

  virtual ~ActiveDirectoryPasswordChangeView() {}

  // Shows the contents of the screen.
  virtual void Show(const std::string& username, int error) = 0;

  // Binds `screen` to the view.
  virtual void Bind(ActiveDirectoryPasswordChangeScreen* screen) = 0;

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

  ActiveDirectoryPasswordChangeScreenHandler(
      JSCallsContainer* js_calls_container);
  ~ActiveDirectoryPasswordChangeScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // ActiveDirectoryPasswordChangeView:
  void Show(const std::string& username, int error) override;
  void Bind(ActiveDirectoryPasswordChangeScreen* screen) override;
  void Unbind() override;
  void ShowSignInError(const std::string& error_text) override;

 private:
  // WebUI message handlers.
  void HandleComplete(const std::string& old_password,
                      const std::string& new_password);

  ActiveDirectoryPasswordChangeScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryPasswordChangeScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
