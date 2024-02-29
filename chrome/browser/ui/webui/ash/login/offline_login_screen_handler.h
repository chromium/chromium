// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class OfflineLoginScreen;

class OfflineLoginView {
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

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<OfflineLoginView> AsWeakPtr() = 0;
};

class OfflineLoginScreenHandler final : public BaseScreenHandler,
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
  base::WeakPtr<OfflineLoginView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<OfflineLoginView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OFFLINE_LOGIN_SCREEN_HANDLER_H_
