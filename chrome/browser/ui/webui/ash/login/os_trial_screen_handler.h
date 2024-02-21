// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class OsTrialScreen;

// Interface for dependency injection between OsTrialScreen and its
// WebUI representation.
class OsTrialScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"os-trial",
                                                       "OsTrialScreen"};

  virtual ~OsTrialScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<OsTrialScreenView> AsWeakPtr() = 0;
};

class OsTrialScreenHandler final : public BaseScreenHandler,
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
  base::WeakPtr<OsTrialScreenView> AsWeakPtr() override;

  base::WeakPtrFactory<OsTrialScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OS_TRIAL_SCREEN_HANDLER_H_
