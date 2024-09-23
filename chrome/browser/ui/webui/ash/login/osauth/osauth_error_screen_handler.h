// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_OSAUTH_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_OSAUTH_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace login {
class LocalizedValuesBuilder;
}

namespace ash {

// Interface for dependency injection between OSAuthErrorScreen and its actual
// representation. Owned by OSAuthErrorScreen.
class OSAuthErrorScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"osauth-error",
                                                       "OSAuthErrorScreen"};

  virtual ~OSAuthErrorScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<OSAuthErrorScreenView> AsWeakPtr() = 0;
};

// A class that handles the WebUI hooks in error screen.
class OSAuthErrorScreenHandler final : public BaseScreenHandler,
                                       public OSAuthErrorScreenView {
 public:
  using TView = OSAuthErrorScreenView;

  OSAuthErrorScreenHandler();

  OSAuthErrorScreenHandler(const OSAuthErrorScreenHandler&) = delete;
  OSAuthErrorScreenHandler& operator=(const OSAuthErrorScreenHandler&) = delete;

  ~OSAuthErrorScreenHandler() override;

 private:
  // OSAuthErrorScreenView:
  void Show() override;
  base::WeakPtr<OSAuthErrorScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<OSAuthErrorScreenHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OSAUTH_OSAUTH_ERROR_SCREEN_HANDLER_H_
