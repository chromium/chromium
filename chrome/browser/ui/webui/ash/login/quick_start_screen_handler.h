// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}

namespace ash {

class QuickStartView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"quick-start",
                                                       "QuickStartScreen"};

  virtual ~QuickStartView() = default;

  virtual void Show() = 0;
  virtual void SetPIN(const std::string pin) = 0;
  virtual void SetQRCode(base::Value::List blob, const std::string url) = 0;
  virtual void ShowInitialUiStep() = 0;
  virtual void ShowBluetoothDialog() = 0;
  virtual void ShowConnectingToPhoneStep() = 0;
  virtual void ShowConnectingToWifi() = 0;
  virtual void ShowConfirmGoogleAccount() = 0;
  virtual void ShowSigningInStep() = 0;
  virtual void ShowCreatingAccountStep() = 0;
  virtual void ShowSetupCompleteStep(const bool did_transfer_wifi) = 0;
  virtual void SetUserEmail(const std::string email) = 0;
  virtual void SetUserFullName(const std::string full_name) = 0;
  virtual void SetUserAvatar(const std::string avatar_url) = 0;
  virtual void SetWillRequestWiFi(const bool will_request_wifi) = 0;
  virtual base::WeakPtr<QuickStartView> AsWeakPtr() = 0;
};

// WebUI implementation of QuickStartView.
class QuickStartScreenHandler final : public QuickStartView,
                                      public BaseScreenHandler {
 public:
  using TView = QuickStartView;

  QuickStartScreenHandler();

  QuickStartScreenHandler(const QuickStartScreenHandler&) = delete;
  QuickStartScreenHandler& operator=(const QuickStartScreenHandler&) = delete;

  ~QuickStartScreenHandler() override;

  // QuickStartView:
  void Show() override;
  void SetPIN(const std::string pin) override;
  void SetQRCode(base::Value::List blob, const std::string url) override;
  void ShowInitialUiStep() override;
  void ShowBluetoothDialog() override;
  void ShowConnectingToPhoneStep() override;
  void ShowConnectingToWifi() override;
  void ShowConfirmGoogleAccount() override;
  void ShowSigningInStep() override;
  void ShowCreatingAccountStep() override;
  void ShowSetupCompleteStep(const bool did_transfer_wifi) override;
  void SetUserEmail(const std::string email) override;
  void SetUserFullName(const std::string full_name) override;
  void SetUserAvatar(const std::string avatar_url) override;
  void SetWillRequestWiFi(const bool will_request_wifi) override;
  base::WeakPtr<QuickStartView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<QuickStartView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_
