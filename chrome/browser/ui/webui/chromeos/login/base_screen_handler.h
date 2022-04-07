// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Base class for the OOBE/Login WebUI handlers which provide methods specific
// to a particular OobeScreen.
class BaseScreenHandler : public BaseWebUIHandler {
 public:
  explicit BaseScreenHandler(OobeScreenId oobe_screen);

  BaseScreenHandler(const BaseScreenHandler&) = delete;
  BaseScreenHandler& operator=(const BaseScreenHandler&) = delete;

  ~BaseScreenHandler() override;

  OobeScreenId oobe_screen() const { return oobe_screen_; }

  // DEPRECATED: To be removed.
  void SetBaseScreenDeprecated(BaseScreen* base_screen);

  // BaseWebUIHandler:
  void RegisterMessages() override;

 protected:
  // Advances to the `oobe_screen_` in the WebUI. Optional `data` will be passed
  // to the `onBeforeShow` on the javascript side.
  void ShowInWebUI(absl::optional<base::Value::Dict> data = absl::nullopt);

  template <typename... Args>
  void CallExternalAPI(const std::string& api_function, Args... args) {
    CallJS<Args...>(GetFullExternalAPIFunctionName(api_function),
                    std::move(args)...);
  }

  // Set the method identifier for a userActed callback. The actual callback
  // will be registered in RegisterMessages so this should be called in the
  // constructor. This takes the full method path, ie,
  // "login.WelcomeScreen.userActed".
  //
  // If this is not called then userActed-style callbacks will not be available
  // for the screen.
  // DEPRECATED: Use 'StaticOobeScreenId::external_api_prefix' instead.
  void set_user_acted_method_path_deprecated(
      const std::string& user_acted_method_path) {
    user_acted_method_path_ = user_acted_method_path;
  }

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

#if DCHECK_IS_ON()
  BaseScreen* base_screen_ = nullptr;
#endif
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
