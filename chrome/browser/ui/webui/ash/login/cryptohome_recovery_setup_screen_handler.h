// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SETUP_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class CryptohomeRecoverySetupScreen;

// Interface for dependency injection between CryptohomeRecoverySetupScreen and
// its WebUI representation.
class CryptohomeRecoverySetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "cryptohome-recovery-setup", "CryptohomeRecoverySetupScreen"};

  virtual ~CryptohomeRecoverySetupScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Shows the recovery setup failed message.
  virtual void OnSetupFailed() = 0;

  // Shows the spinner in the UI.
  virtual void SetLoadingState() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<CryptohomeRecoverySetupScreenView> AsWeakPtr() = 0;
};

class CryptohomeRecoverySetupScreenHandler final
    : public CryptohomeRecoverySetupScreenView,
      public BaseScreenHandler {
 public:
  using TView = CryptohomeRecoverySetupScreenView;

  CryptohomeRecoverySetupScreenHandler();

  CryptohomeRecoverySetupScreenHandler(
      const CryptohomeRecoverySetupScreenHandler&) = delete;
  CryptohomeRecoverySetupScreenHandler& operator=(
      const CryptohomeRecoverySetupScreenHandler&) = delete;

  ~CryptohomeRecoverySetupScreenHandler() override;

 private:
  // CryptohomeRecoverySetupScreenView
  void Show() override;
  void OnSetupFailed() override;
  void SetLoadingState() override;
  base::WeakPtr<CryptohomeRecoverySetupScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<CryptohomeRecoverySetupScreenView> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SETUP_SCREEN_HANDLER_H_
