// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between ResetScreen and its actual
// representation, either views based or WebUI.
class ResetView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"reset", "ResetScreen"};

  virtual ~ResetView() = default;

  virtual void Show() = 0;
  virtual void SetIsRollbackAvailable(bool value) = 0;
  virtual void SetIsRollbackRequested(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateAvailable(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateEditable(bool value) = 0;
  virtual void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) = 0;
  virtual void SetShouldShowConfirmationDialog(bool value) = 0;
  virtual void SetScreenState(int value) = 0;
  virtual base::WeakPtr<ResetView> AsWeakPtr() = 0;
};

class ResetScreenHandler final : public ResetView, public BaseScreenHandler {
 public:
  using TView = ResetView;

  ResetScreenHandler();

  ResetScreenHandler(const ResetScreenHandler&) = delete;
  ResetScreenHandler& operator=(const ResetScreenHandler&) = delete;

  ~ResetScreenHandler() override;

 private:
  // ResetView:
  void Show() override;
  void SetIsRollbackAvailable(bool value) override;
  void SetIsRollbackRequested(bool value) override;
  void SetIsTpmFirmwareUpdateAvailable(bool value) override;
  void SetIsTpmFirmwareUpdateChecked(bool value) override;
  void SetIsTpmFirmwareUpdateEditable(bool value) override;
  void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) override;
  void SetShouldShowConfirmationDialog(bool value) override;
  void SetScreenState(int value) override;
  base::WeakPtr<ResetView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<ResetView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_
