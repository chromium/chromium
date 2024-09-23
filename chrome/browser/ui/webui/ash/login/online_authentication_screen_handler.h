// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_AUTHENTICATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_AUTHENTICATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class OnlineAuthenticationScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "online-authentication-screen", "OnlineAuthenticationScreen"};

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual base::WeakPtr<OnlineAuthenticationScreenView> AsWeakPtr() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class OnlineAuthenticationScreenHandler final
    : public OnlineAuthenticationScreenView,
      public BaseScreenHandler {
 public:
  using TView = OnlineAuthenticationScreenView;

  OnlineAuthenticationScreenHandler();

  OnlineAuthenticationScreenHandler(const OnlineAuthenticationScreenHandler&) =
      delete;
  OnlineAuthenticationScreenHandler& operator=(
      const OnlineAuthenticationScreenHandler&) = delete;

  ~OnlineAuthenticationScreenHandler() override;

  void Show() override;
  void Hide() override;
  base::WeakPtr<OnlineAuthenticationScreenView> AsWeakPtr() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;

  base::WeakPtrFactory<OnlineAuthenticationScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ONLINE_AUTHENTICATION_SCREEN_HANDLER_H_
