// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class OfflineLoginScreen;
}

namespace chromeos {

class OfflineLoginView : public base::SupportsWeakPtr<OfflineLoginView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"offline-login",
                                                       "OfflineLoginScreen"};

  OfflineLoginView() = default;
  virtual ~OfflineLoginView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict params) = 0;

  // Hide the contents of the screen.
  virtual void Hide() = 0;

  // Clear the input fields on the screen.
  virtual void Reset() = 0;

  // Proceeds to the password input dialog.
  virtual void ShowPasswordPage() = 0;

  // Shows error pop-up when the user cannot login offline.
  virtual void ShowOnlineRequiredDialog() = 0;

  // Shows error message for not matching email/password pair.
  virtual void ShowPasswordMismatchMessage() = 0;
};

class OfflineLoginScreenHandler : public BaseScreenHandler,
                                  public OfflineLoginView {
 public:
  using TView = OfflineLoginView;
  OfflineLoginScreenHandler();
  ~OfflineLoginScreenHandler() override;

  OfflineLoginScreenHandler(const OfflineLoginScreenHandler&) = delete;
  OfflineLoginScreenHandler& operator=(const OfflineLoginScreenHandler&) =
      delete;

 private:
  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);

  // OfflineLoginView:
  void Show(base::Value::Dict params) override;
  void Hide() override;
  void Reset() override;
  void ShowPasswordPage() override;
  void ShowOnlineRequiredDialog() override;
  void ShowPasswordMismatchMessage() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::OfflineLoginScreenHandler;
using ::chromeos::OfflineLoginView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_
