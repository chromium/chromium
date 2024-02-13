// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between MultiDeviceSetupScreen and its
// WebUI representation.
class MultiDeviceSetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "multidevice-setup-screen", "MultiDeviceSetupScreen"};

  virtual ~MultiDeviceSetupScreenView() = default;

  virtual void Show() = 0;
  virtual base::WeakPtr<MultiDeviceSetupScreenView> AsWeakPtr() = 0;
};

// Concrete MultiDeviceSetupScreenView WebUI-based implementation.
class MultiDeviceSetupScreenHandler final : public BaseScreenHandler,
                                            public MultiDeviceSetupScreenView {
 public:
  using TView = MultiDeviceSetupScreenView;

  MultiDeviceSetupScreenHandler();

  MultiDeviceSetupScreenHandler(const MultiDeviceSetupScreenHandler&) = delete;
  MultiDeviceSetupScreenHandler& operator=(
      const MultiDeviceSetupScreenHandler&) = delete;

  ~MultiDeviceSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

  // MultiDeviceSetupScreenView:
  void Show() override;
  base::WeakPtr<MultiDeviceSetupScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<MultiDeviceSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_MULTIDEVICE_SETUP_SCREEN_HANDLER_H_
