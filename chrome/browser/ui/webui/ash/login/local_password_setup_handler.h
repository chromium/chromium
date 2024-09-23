// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCAL_PASSWORD_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCAL_PASSWORD_SETUP_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class LocalPasswordSetupView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "local-password-setup", "LocalPasswordSetupScreen"};

  LocalPasswordSetupView() = default;

  LocalPasswordSetupView(const LocalPasswordSetupView&) = delete;
  LocalPasswordSetupView& operator=(const LocalPasswordSetupView&) = delete;

  virtual void Show(bool can_go_back, bool is_recovery_flow) = 0;
  virtual void ShowLocalPasswordSetupFailure() = 0;
  virtual base::WeakPtr<LocalPasswordSetupView> AsWeakPtr() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class LocalPasswordSetupHandler final : public BaseScreenHandler,
                                        public LocalPasswordSetupView {
 public:
  using TView = LocalPasswordSetupView;

  LocalPasswordSetupHandler();

  LocalPasswordSetupHandler(const LocalPasswordSetupHandler&) = delete;
  LocalPasswordSetupHandler& operator=(const LocalPasswordSetupHandler&) =
      delete;

  ~LocalPasswordSetupHandler() override;

  // LocalPasswordSetupView:
  void Show(bool can_go_back, bool is_recovery_flow) override;
  void ShowLocalPasswordSetupFailure() override;
  base::WeakPtr<LocalPasswordSetupView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(::login::LocalizedValuesBuilder* builder) final;

 private:
  base::WeakPtrFactory<LocalPasswordSetupView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_LOCAL_PASSWORD_SETUP_HANDLER_H_
