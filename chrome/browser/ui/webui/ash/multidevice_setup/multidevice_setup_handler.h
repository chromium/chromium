// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash::multidevice_setup {

// Chrome MultiDevice setup flow WebUI handler.
class MultideviceSetupHandler : public content::WebUIMessageHandler {
 public:
  // `user` must be non-null and must outlive `this`.
  explicit MultideviceSetupHandler(const user_manager::User* user);

  MultideviceSetupHandler(const MultideviceSetupHandler&) = delete;
  MultideviceSetupHandler& operator=(const MultideviceSetupHandler&) = delete;

  ~MultideviceSetupHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleGetProfileInfo(const base::ListValue& args);
  void HandleOpenMultiDeviceSettings(const base::ListValue& args);

  void OnGetAuthFactorsConfiguration(std::unique_ptr<UserContext> user_context,
                                     std::optional<AuthenticationError> error);

  AuthFactorEditor auth_factor_editor_;
  const raw_ref<const user_manager::User> user_;
  // Whether the user has only pin factor and should be authenticated by pin.
  bool authenticate_by_pin_ = false;
};

}  // namespace ash::multidevice_setup

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_HANDLER_H_
