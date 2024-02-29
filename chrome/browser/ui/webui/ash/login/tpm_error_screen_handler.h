// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between TpmErrorScreen and its
// WebUI representation.
class TpmErrorView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"tpm-error-message",
                                                       "TPMErrorMessageScreen"};

  virtual ~TpmErrorView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Sets corresponding error message when taking tpm ownership return an error.
  virtual void SetTPMOwnedErrorStep() = 0;
  virtual void SetTPMDbusErrorStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<TpmErrorView> AsWeakPtr() = 0;
};

class TpmErrorScreenHandler final : public TpmErrorView,
                                    public BaseScreenHandler {
 public:
  using TView = TpmErrorView;

  TpmErrorScreenHandler();
  TpmErrorScreenHandler(const TpmErrorScreenHandler&) = delete;
  TpmErrorScreenHandler& operator=(const TpmErrorScreenHandler&) = delete;
  ~TpmErrorScreenHandler() override;

 private:
  void Show() override;
  void SetTPMOwnedErrorStep() override;
  void SetTPMDbusErrorStep() override;
  base::WeakPtr<TpmErrorView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<TpmErrorView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TPM_ERROR_SCREEN_HANDLER_H_
