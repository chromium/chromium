// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCAL_STATE_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCAL_STATE_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class LocalStateErrorScreenView
    : public base::SupportsWeakPtr<LocalStateErrorScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"local-state-error",
                                                       "LocalStateErrorScreen"};

  virtual ~LocalStateErrorScreenView() = default;

  virtual void Show() = 0;
};

class LocalStateErrorScreenHandler : public LocalStateErrorScreenView,
                                     public BaseScreenHandler {
 public:
  using TView = LocalStateErrorScreenView;

  LocalStateErrorScreenHandler();

  LocalStateErrorScreenHandler(const LocalStateErrorScreenHandler&) = delete;
  LocalStateErrorScreenHandler& operator=(const LocalStateErrorScreenHandler&) =
      delete;

  ~LocalStateErrorScreenHandler() override;

 private:
  // LocalStateErrorScreenView:
  void Show() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LocalStateErrorScreenHandler;
using ::chromeos::LocalStateErrorScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCAL_STATE_ERROR_SCREEN_HANDLER_H_
