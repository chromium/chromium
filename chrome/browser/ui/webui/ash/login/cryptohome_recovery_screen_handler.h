// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class CryptohomeRecoveryScreen;

// Interface for dependency injection between CryptohomeRecoveryScreen and its
// WebUI representation.
class CryptohomeRecoveryScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "cryptohome-recovery", "CryptohomeRecoveryScreen"};

  virtual ~CryptohomeRecoveryScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Shows the reauth required message when there's no reauth proof token.
  virtual void ShowReauthNotification() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<CryptohomeRecoveryScreenView> AsWeakPtr() = 0;
};

class CryptohomeRecoveryScreenHandler final
    : public CryptohomeRecoveryScreenView,
      public BaseScreenHandler {
 public:
  using TView = CryptohomeRecoveryScreenView;

  CryptohomeRecoveryScreenHandler();

  CryptohomeRecoveryScreenHandler(const CryptohomeRecoveryScreenHandler&) =
      delete;
  CryptohomeRecoveryScreenHandler& operator=(
      const CryptohomeRecoveryScreenHandler&) = delete;

  ~CryptohomeRecoveryScreenHandler() override;

 private:
  // CryptohomeRecoveryScreenView
  void Show() override;
  void ShowReauthNotification() override;
  base::WeakPtr<CryptohomeRecoveryScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<CryptohomeRecoveryScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CRYPTOHOME_RECOVERY_SCREEN_HANDLER_H_
