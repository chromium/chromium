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

class QuickStartView : public base::SupportsWeakPtr<QuickStartView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"quick-start",
                                                       "QuickStartScreen"};

  virtual ~QuickStartView() = default;

  virtual void Show() = 0;
  virtual void SetPIN(const std::string pin) = 0;
  virtual void SetQRCode(base::Value::List blob) = 0;
  virtual void SetDiscoverableName(const std::string& discoverable_name) = 0;
  virtual void ShowConnectingToWifi() = 0;
  virtual void ShowTransferringGaiaCredentials() = 0;
  virtual void ShowFidoAssertionReceived(std::string email) = 0;
};

// WebUI implementation of QuickStartView.
class QuickStartScreenHandler : public QuickStartView,
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
  void SetQRCode(base::Value::List blob) override;
  void SetDiscoverableName(const std::string& discoverable_name) override;
  void ShowConnectingToWifi() override;
  void ShowTransferringGaiaCredentials() override;
  void ShowFidoAssertionReceived(std::string email) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_QUICK_START_SCREEN_HANDLER_H_
