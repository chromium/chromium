// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class OsTrialScreen;
}

namespace chromeos {

// Interface for dependency injection between OsTrialScreen and its
// WebUI representation.
class OsTrialScreenView : public base::SupportsWeakPtr<OsTrialScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"os-trial",
                                                       "OsTrialScreen"};

  virtual ~OsTrialScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
};

class OsTrialScreenHandler : public BaseScreenHandler,
                             public OsTrialScreenView {
 public:
  using TView = OsTrialScreenView;

  OsTrialScreenHandler();
  OsTrialScreenHandler(const OsTrialScreenHandler&) = delete;
  OsTrialScreenHandler& operator=(const OsTrialScreenHandler&) = delete;
  ~OsTrialScreenHandler() override;

 private:
  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // OsTrialScreenView:
  void Show() override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::OsTrialScreenHandler;
using ::chromeos::OsTrialScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_
