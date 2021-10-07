// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class TpmErrorScreen;
}

namespace chromeos {

// Interface for dependency injection between TpmErrorScreen and its
// WebUI representation.
class TpmErrorView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"tpm-error-message"};

  virtual ~TpmErrorView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Binds `screen` to the view.
  virtual void Bind(ash::TpmErrorScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Sets corresponding error message when tpm is owned.
  virtual void SetTPMOwnedErrorStep(bool show_tpm_owned_step) = 0;

  // Sets if build is branded or not to show correct error message when TPM is
  // owned by other OS.
  virtual void SetIsBrandedBuild(bool is_branded) = 0;
};

class TpmErrorScreenHandler : public TpmErrorView, public BaseScreenHandler {
 public:
  using TView = TpmErrorView;

  explicit TpmErrorScreenHandler(JSCallsContainer* js_calls_container);
  TpmErrorScreenHandler(const TpmErrorScreenHandler&) = delete;
  TpmErrorScreenHandler& operator=(const TpmErrorScreenHandler&) = delete;
  ~TpmErrorScreenHandler() override;

 private:
  void Show() override;
  void Bind(ash::TpmErrorScreen* screen) override;
  void Unbind() override;
  void SetTPMOwnedErrorStep(bool show_tpm_owned_step) override;
  void SetIsBrandedBuild(bool is_branded) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  bool show_on_init_ = false;

  ash::TpmErrorScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::TpmErrorScreenHandler;
using ::chromeos::TpmErrorView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
