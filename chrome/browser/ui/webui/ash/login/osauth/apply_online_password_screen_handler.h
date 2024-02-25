// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class ApplyOnlinePasswordScreen;

// Interface for dependency injection between ApplyOnlinePasswordScreen and
// its WebUI representation.
class ApplyOnlinePasswordScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "apply-online-password", "ApplyOnlinePasswordScreen"};

  virtual ~ApplyOnlinePasswordScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
  virtual base::WeakPtr<ApplyOnlinePasswordScreenView> AsWeakPtr() = 0;
};

class ApplyOnlinePasswordScreenHandler final
    : public ApplyOnlinePasswordScreenView,
      public BaseScreenHandler {
 public:
  using TView = ApplyOnlinePasswordScreenView;

  ApplyOnlinePasswordScreenHandler();

  ApplyOnlinePasswordScreenHandler(const ApplyOnlinePasswordScreenHandler&) =
      delete;
  ApplyOnlinePasswordScreenHandler& operator=(
      const ApplyOnlinePasswordScreenHandler&) = delete;

  ~ApplyOnlinePasswordScreenHandler() override;

 private:
  // ApplyOnlinePasswordScreenView
  void Show() override;
  base::WeakPtr<ApplyOnlinePasswordScreenView> AsWeakPtr() override;

  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override {}

  base::WeakPtrFactory<ApplyOnlinePasswordScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_HANDLER_H_
