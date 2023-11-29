// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class LocalDataLossWarningScreenView
    : public base::SupportsWeakPtr<LocalDataLossWarningScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "local-data-loss-warning", "LocalDataLossWarningScreen"};

  LocalDataLossWarningScreenView() = default;

  LocalDataLossWarningScreenView(const LocalDataLossWarningScreenView&) =
      delete;
  LocalDataLossWarningScreenView& operator=(
      const LocalDataLossWarningScreenView&) = delete;

  virtual void Show(const std::string& email) = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class LocalDataLossWarningScreenHandler
    : public BaseScreenHandler,
      public LocalDataLossWarningScreenView {
 public:
  using TView = LocalDataLossWarningScreenView;

  LocalDataLossWarningScreenHandler();

  LocalDataLossWarningScreenHandler(const LocalDataLossWarningScreenHandler&) =
      delete;
  LocalDataLossWarningScreenHandler& operator=(
      const LocalDataLossWarningScreenHandler&) = delete;

  ~LocalDataLossWarningScreenHandler() override;

  // LocalDataLossWarningView:
  void Show(const std::string& email) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(::login::LocalizedValuesBuilder* builder) final;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_HANDLER_H_
