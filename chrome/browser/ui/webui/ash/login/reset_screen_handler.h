// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between ResetScreen and its actual
// representation, either views based or WebUI.
class ResetView : public base::SupportsWeakPtr<ResetView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"reset", "ResetScreen"};

  virtual ~ResetView() = default;

  virtual void Show() = 0;

  enum class State {
    kRestartRequired = 0,
    kRevertPromise,
    kPowerwashProposal,
    kError,
  };

  virtual void SetIsRollbackAvailable(bool value) = 0;
  virtual void SetIsRollbackRequested(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateAvailable(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateEditable(bool value) = 0;
  virtual void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) = 0;
  virtual void SetShouldShowConfirmationDialog(bool value) = 0;
  virtual void SetConfirmationDialogClosed() = 0;
  virtual void SetScreenState(State value) = 0;

  virtual State GetScreenState() = 0;
  virtual tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() = 0;
  virtual bool GetIsRollbackAvailable() = 0;
  virtual bool GetIsRollbackRequested() = 0;
  virtual bool GetIsTpmFirmwareUpdateChecked() = 0;
};

// WebUI implementation of ResetScreenActor.
class ResetScreenHandler : public ResetView,
                           public BaseScreenHandler {
 public:
  using TView = ResetView;

  ResetScreenHandler();

  ResetScreenHandler(const ResetScreenHandler&) = delete;
  ResetScreenHandler& operator=(const ResetScreenHandler&) = delete;

  ~ResetScreenHandler() override;

  void Show() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void SetIsRollbackAvailable(bool value) override;
  void SetIsRollbackRequested(bool value) override;
  void SetIsTpmFirmwareUpdateAvailable(bool value) override;
  void SetIsTpmFirmwareUpdateChecked(bool value) override;
  void SetIsTpmFirmwareUpdateEditable(bool value) override;
  void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) override;
  void SetShouldShowConfirmationDialog(bool value) override;
  void SetConfirmationDialogClosed() override;
  void SetScreenState(State value) override;
  State GetScreenState() override;
  tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() override;
  bool GetIsRollbackAvailable() override;
  bool GetIsRollbackRequested() override;
  bool GetIsTpmFirmwareUpdateChecked() override;

 private:
  void HandleSetTpmFirmwareUpdateChecked(bool value);

  ResetView::State state_ = ResetView::State::kRestartRequired;
  tpm_firmware_update::Mode mode_ = tpm_firmware_update::Mode::kNone;
  bool is_rollback_available_ = false;
  bool is_rollback_requested_ = false;
  bool is_tpm_firmware_update_checked_ = false;
  bool is_showing_confirmation_dialog_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RESET_SCREEN_HANDLER_H_
