// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace ash {
class FingerprintSetupScreen;
}

namespace chromeos {

// Interface for dependency injection between FingerprintSetupScreen and its
// WebUI representation.
class FingerprintSetupScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"fingerprint-setup"};

  virtual ~FingerprintSetupScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(ash::FingerprintSetupScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Enables adding new finger.
  virtual void EnableAddAnotherFinger(bool enable) = 0;

  // Trigger update UI state due to enroll status update.
  virtual void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                bool enroll_session_complete,
                                int percent_complete) = 0;
};

// The sole implementation of the FingerprintSetupScreenView, using WebUI.
class FingerprintSetupScreenHandler : public BaseScreenHandler,
                                      public FingerprintSetupScreenView {
 public:
  using TView = FingerprintSetupScreenView;

  FingerprintSetupScreenHandler();

  FingerprintSetupScreenHandler(const FingerprintSetupScreenHandler&) = delete;
  FingerprintSetupScreenHandler& operator=(
      const FingerprintSetupScreenHandler&) = delete;

  ~FingerprintSetupScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // FingerprintSetupScreenView:
  void Bind(ash::FingerprintSetupScreen* screen) override;
  void Show() override;
  void Hide() override;
  void EnableAddAnotherFinger(bool enable) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;

  // BaseScreenHandler:
  void InitializeDeprecated() override;

 private:
  ash::FingerprintSetupScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FingerprintSetupScreenHandler;
using ::chromeos::FingerprintSetupScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
