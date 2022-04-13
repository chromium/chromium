// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"

#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

namespace {

constexpr char kUsernameKey[] = "username";
constexpr char kErrorKey[] = "error";

}  // namespace

constexpr StaticOobeScreenId ActiveDirectoryPasswordChangeView::kScreenId;

ActiveDirectoryPasswordChangeScreenHandler::
    ActiveDirectoryPasswordChangeScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.ActiveDirectoryPasswordChangeScreen.userActed");
}

ActiveDirectoryPasswordChangeScreenHandler::
    ~ActiveDirectoryPasswordChangeScreenHandler() {}

void ActiveDirectoryPasswordChangeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adPassChangeMessage", IDS_AD_PASSWORD_CHANGE_MESSAGE);
}

void ActiveDirectoryPasswordChangeScreenHandler::InitializeDeprecated() {}

void ActiveDirectoryPasswordChangeScreenHandler::Show(
    const std::string& username,
    int error) {
  base::Value::Dict data;
  data.Set(kUsernameKey, username);
  data.Set(kErrorKey, error);
  ShowInWebUI(std::move(data));
}

void ActiveDirectoryPasswordChangeScreenHandler::Bind(
    ActiveDirectoryPasswordChangeScreen* screen) {
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
}

void ActiveDirectoryPasswordChangeScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

void ActiveDirectoryPasswordChangeScreenHandler::ShowSignInError(
    const std::string& error_text) {
  CallJS("login.ActiveDirectoryPasswordChangeScreen.showErrorDialog",
         error_text);
}

}  // namespace chromeos
