// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}

namespace ash {

// Interface for dependency injection between FactorSetupSuccessScreen and its
// actual representation. Owned by FactorSetupSuccessScreen.
class FactorSetupSuccessScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "factor-setup-success", "FactorSetupSuccessScreen"};

  virtual ~FactorSetupSuccessScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict params) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FactorSetupSuccessScreenView> AsWeakPtr() = 0;
};

// A class that handles the WebUI hooks in error screen.
class FactorSetupSuccessScreenHandler final
    : public BaseScreenHandler,
      public FactorSetupSuccessScreenView {
 public:
  using TView = FactorSetupSuccessScreenView;

  FactorSetupSuccessScreenHandler();

  FactorSetupSuccessScreenHandler(const FactorSetupSuccessScreenHandler&) =
      delete;
  FactorSetupSuccessScreenHandler& operator=(
      const FactorSetupSuccessScreenHandler&) = delete;

  ~FactorSetupSuccessScreenHandler() override;

 private:
  // FactorSetupSuccessScreenView:
  void Show(base::Value::Dict params) override;
  base::WeakPtr<FactorSetupSuccessScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<FactorSetupSuccessScreenHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_HANDLER_H_
