// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between GaiaPasswordChangedScreen and its
// WebUI representation.
class GaiaPasswordChangedView
    : public base::SupportsWeakPtr<GaiaPasswordChangedView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "gaia-password-changed", "GaiaPasswordChangedScreen"};

  virtual ~GaiaPasswordChangedView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::string& email, bool has_error) = 0;
};

class GaiaPasswordChangedScreenHandler : public GaiaPasswordChangedView,
                                         public BaseScreenHandler {
 public:
  using TView = GaiaPasswordChangedView;

  GaiaPasswordChangedScreenHandler();
  GaiaPasswordChangedScreenHandler(const GaiaPasswordChangedScreenHandler&) =
      delete;
  GaiaPasswordChangedScreenHandler& operator=(
      const GaiaPasswordChangedScreenHandler&) = delete;
  ~GaiaPasswordChangedScreenHandler() override;

 private:
  void Show(const std::string& email, bool has_error) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GAIA_PASSWORD_CHANGED_SCREEN_HANDLER_H_
