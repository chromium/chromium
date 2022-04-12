// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_LOGIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_LOGIN_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface for dependency injection between ActiveDirectoryLoginScreen and its
// WebUI representation.
class ActiveDirectoryLoginView
    : public base::SupportsWeakPtr<ActiveDirectoryLoginView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "offline-ad-login", "ActiveDirectoryLoginScreen"};

  virtual ~ActiveDirectoryLoginView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Clear the input fields on the screen.
  virtual void Reset() = 0;

  // Set error state.
  virtual void SetErrorState(const std::string& username, int errorState) = 0;
};

class ActiveDirectoryLoginScreenHandler : public ActiveDirectoryLoginView,
                                          public BaseScreenHandler {
 public:
  using TView = ActiveDirectoryLoginView;

  ActiveDirectoryLoginScreenHandler();

  ~ActiveDirectoryLoginScreenHandler() override;

  ActiveDirectoryLoginScreenHandler(const ActiveDirectoryLoginScreenHandler&) =
      delete;
  ActiveDirectoryLoginScreenHandler& operator=(
      const ActiveDirectoryLoginScreenHandler&) = delete;

 private:
  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);

  // ActiveDirectoryLoginView:
  void Show() override;
  void Reset() override;
  void SetErrorState(const std::string& username, int errorState) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ActiveDirectoryLoginScreenHandler;
using ::chromeos::ActiveDirectoryLoginView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_LOGIN_SCREEN_HANDLER_H_
