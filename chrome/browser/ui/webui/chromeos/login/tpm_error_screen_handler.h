// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class TpmErrorScreen;

// Interface for dependency injection between TpmErrorScreen and its
// WebUI representation.
class TpmErrorView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"tpm-error-message"};

  virtual ~TpmErrorView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Binds `screen` to the view.
  virtual void Bind(TpmErrorScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
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
  void Bind(TpmErrorScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  bool show_on_init_ = false;

  TpmErrorScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
