// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between PinSetupScreen and its
// WebUI representation.
class PinSetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"pin-setup",
                                                       "PinSetupScreen"};

  virtual ~PinSetupScreenView() = default;

  // Shows the contents of the screen, using |token| to access QuickUnlock API.
  virtual void Show(const std::string& token,
                    bool is_child_account,
                    bool has_login_support,
                    bool using_pin_as_main_factor) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<PinSetupScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the PinSetupScreenView, using WebUI.
class PinSetupScreenHandler final : public BaseScreenHandler,
                                    public PinSetupScreenView {
 public:
  using TView = PinSetupScreenView;

  PinSetupScreenHandler();

  PinSetupScreenHandler(const PinSetupScreenHandler&) = delete;
  PinSetupScreenHandler& operator=(const PinSetupScreenHandler&) = delete;

  ~PinSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // PinSetupScreenView:
  void Show(const std::string& token,
            bool is_child_account,
            bool has_login_support,
            bool using_pin_as_main_factor) override;

  base::WeakPtr<PinSetupScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<PinSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PIN_SETUP_SCREEN_HANDLER_H_
