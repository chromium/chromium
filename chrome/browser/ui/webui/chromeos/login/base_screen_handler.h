// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

class BaseScreen;

// Base class for the OOBE/Login WebUI handlers which provide methods specific
// to a particular OobeScreen.
class BaseScreenHandler : public BaseWebUIHandler {
 public:
  BaseScreenHandler(OobeScreenId oobe_screen,
                    JSCallsContainer* js_calls_container);
  ~BaseScreenHandler() override;

  OobeScreenId oobe_screen() const { return oobe_screen_; }

  void SetBaseScreen(BaseScreen* base_screen);

  // BaseWebUIHandler:
  void RegisterMessages() override;

 protected:
  // Set the method identifier for a userActed callback. The actual callback
  // will be registered in RegisterMessages so this should be called in the
  // constructor. This takes the full method path, ie,
  // "login.WelcomeScreen.userActed".
  //
  // If this is not called then userActed-style callbacks will not be available
  // for the screen.
  void set_user_acted_method_path(const std::string& user_acted_method_path) {
    user_acted_method_path_ = user_acted_method_path;
  }

 private:
  // Handles user action.
  void HandleUserAction(const std::string& action_id);

  // Path that is used to invoke user actions.
  std::string user_acted_method_path_;

  // OobeScreen that this handler corresponds to.
  OobeScreenId oobe_screen_ = OobeScreen::SCREEN_UNKNOWN;

  BaseScreen* base_screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BaseScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
