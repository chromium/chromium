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

class OfflineLoginView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"offline-login"};

  OfflineLoginView() = default;
  virtual ~OfflineLoginView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hide the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(ash::OfflineLoginScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Clear the input fields on the screen.
  virtual void Reset() = 0;

  // Preload e-mail, enterprise domain and e-mail domain.
  // TODO(dkuzmin): merge this function with Show() in future and use
  // ShowScreenWithData in handler.
  virtual void LoadParams(base::DictionaryValue params) = 0;

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
  void HandleEmailSubmitted(const std::string& username);

  // OfflineLoginView:
  void Show() override;
  void Hide() override;
  void Bind(ash::OfflineLoginScreen* screen) override;
  void Unbind() override;
  void Reset() override;
  void LoadParams(base::DictionaryValue params) override;
  void ShowPasswordPage() override;
  void ShowOnlineRequiredDialog() override;
  void ShowPasswordMismatchMessage() override;

  // BaseScreenHandler:
  void RegisterMessages() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

  ash::OfflineLoginScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::OfflineLoginScreenHandler;
using ::chromeos::OfflineLoginView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_
