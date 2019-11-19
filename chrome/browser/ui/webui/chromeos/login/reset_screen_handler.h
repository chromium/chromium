// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

class ResetScreen;

// Interface for dependency injection between ResetScreen and its actual
// representation, either views based or WebUI.
class ResetView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"reset"};

  virtual ~ResetView() {}

  virtual void Bind(ResetScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

  enum class State {
    kRestartRequired = 0,
    kRevertPromise,
    kPowerwashProposal,
    kError,
  };

  virtual void SetIsRollbackAvailable(bool value) = 0;
  virtual void SetIsRollbackChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateAvailable(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateEditable(bool value) = 0;
  virtual void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) = 0;
  virtual void SetIsConfirmational(bool value) = 0;
  virtual void SetIsOfficialBuild(bool value) = 0;
  virtual void SetScreenState(State value) = 0;

  virtual State GetScreenState() = 0;
  virtual tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() = 0;
  virtual bool GetIsRollbackAvailable() = 0;
  virtual bool GetIsRollbackChecked() = 0;
  virtual bool GetIsTpmFirmwareUpdateChecked() = 0;
};

// WebUI implementation of ResetScreenActor.
class ResetScreenHandler : public ResetView,
                           public BaseScreenHandler {
 public:
  using TView = ResetView;

  explicit ResetScreenHandler(JSCallsContainer* js_calls_container);
  ~ResetScreenHandler() override;

  // ResetView implementation:
  void Bind(ResetScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void Initialize() override;
  void SetIsRollbackAvailable(bool value) override;
  void SetIsRollbackChecked(bool value) override;
  void SetIsTpmFirmwareUpdateAvailable(bool value) override;
  void SetIsTpmFirmwareUpdateChecked(bool value) override;
  void SetIsTpmFirmwareUpdateEditable(bool value) override;
  void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) override;
  void SetIsConfirmational(bool value) override;
  void SetIsOfficialBuild(bool value) override;
  void SetScreenState(State value) override;
  State GetScreenState() override;
  tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() override;
  bool GetIsRollbackAvailable() override;
  bool GetIsRollbackChecked() override;
  bool GetIsTpmFirmwareUpdateChecked() override;

 private:
  void HandleSetTpmFirmwareUpdateChecked(bool value);

  ResetScreen* screen_ = nullptr;

  // If true, Initialize() will call Show().
  bool show_on_init_ = false;

  ResetView::State state_ = ResetView::State::kRestartRequired;
  tpm_firmware_update::Mode mode_ = tpm_firmware_update::Mode::kNone;
  bool is_rollback_available_ = false;
  bool is_rollback_checked_ = false;
  bool is_tpm_firmware_update_checked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ResetScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RESET_SCREEN_HANDLER_H_
