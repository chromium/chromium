// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_SCREEN_HANDLER_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

namespace ash {

// Base class for the OOBE/Login WebUI handlers which provide methods specific
// to a particular OobeScreen.
class BaseScreenHandler : public BaseWebUIHandler {
 public:
  explicit BaseScreenHandler(OobeScreenId oobe_screen);

  BaseScreenHandler(const BaseScreenHandler&) = delete;
  BaseScreenHandler& operator=(const BaseScreenHandler&) = delete;

  ~BaseScreenHandler() override;

  OobeScreenId oobe_screen() const { return oobe_screen_; }

  // BaseWebUIHandler:
  void RegisterMessages() final;

 protected:
  // Advances to the `oobe_screen_` in the WebUI. Optional `data` will be passed
  // to the `onBeforeShow` on the javascript side.
  void ShowInWebUI(std::optional<base::Value::Dict> data = std::nullopt);

  template <typename... Args>
  void CallExternalAPI(const std::string& api_function, Args... args) {
    CallJS<Args...>(GetFullExternalAPIFunctionName(api_function),
                    std::move(args)...);
  }

  bool HandleUserActionImpl(const base::Value::List& args);

 private:
  // Handles user action.
  void HandleUserAction(const base::Value::List& args);

  // Generates the full function name to call an API function of the screen.
  // `oobe_screen_.external_api_prefix` must be set.
  std::string GetFullExternalAPIFunctionName(const std::string& short_name);

  // Path that is used to invoke user actions.
  std::string user_acted_method_path_;

  // OobeScreen that this handler corresponds to.
  const OobeScreenId oobe_screen_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_SCREEN_HANDLER_H_
