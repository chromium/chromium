// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace ash {

// Interface for dependency injection between FingerprintSetupScreen and its
// WebUI representation.
class FingerprintSetupScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "fingerprint-setup", "FingerprintSetupScreen"};

  virtual ~FingerprintSetupScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Enables adding new finger.
  virtual void EnableAddAnotherFinger(bool enable) = 0;

  // Trigger update UI state due to enroll status update.
  virtual void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                bool enroll_session_complete,
                                int percent_complete) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FingerprintSetupScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the FingerprintSetupScreenView, using WebUI.
class FingerprintSetupScreenHandler final : public BaseScreenHandler,
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

  // FingerprintSetupScreenView:
  void Show() override;
  void EnableAddAnotherFinger(bool enable) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;
  base::WeakPtr<FingerprintSetupScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FingerprintSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FINGERPRINT_SETUP_SCREEN_HANDLER_H_
