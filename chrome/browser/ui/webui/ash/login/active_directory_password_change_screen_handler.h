// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class ActiveDirectoryPasswordChangeScreen;

// Interface for dependency injection between
// ActiveDirectoryPasswordChangeScreen and its WebUI representation.
class ActiveDirectoryPasswordChangeView
    : public base::SupportsWeakPtr<ActiveDirectoryPasswordChangeView> {
 public:
  inline static constexpr StaticOobeScreenId kScreenId{
      "ad-password-change", "ActiveDirectoryPasswordChangeScreen"};

  virtual ~ActiveDirectoryPasswordChangeView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::string& username, int error) = 0;

  // Shows sign-in error bubble.
  virtual void ShowSignInError(const std::string& error_text) = 0;
};

// A class that handles WebUI hooks in Active Directory password change screen.
class ActiveDirectoryPasswordChangeScreenHandler
    : public ActiveDirectoryPasswordChangeView,
      public BaseScreenHandler {
 public:
  using TView = ActiveDirectoryPasswordChangeView;

  ActiveDirectoryPasswordChangeScreenHandler();

  ActiveDirectoryPasswordChangeScreenHandler(
      const ActiveDirectoryPasswordChangeScreenHandler&) = delete;
  ActiveDirectoryPasswordChangeScreenHandler& operator=(
      const ActiveDirectoryPasswordChangeScreenHandler&) = delete;

  ~ActiveDirectoryPasswordChangeScreenHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // ActiveDirectoryPasswordChangeView:
  void Show(const std::string& username, int error) override;
  void ShowSignInError(const std::string& error_text) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_HANDLER_H_
