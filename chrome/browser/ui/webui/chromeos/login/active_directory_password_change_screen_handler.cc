// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"

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
    ActiveDirectoryPasswordChangeScreenHandler(
        JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path(
      "login.ActiveDirectoryPasswordChangeScreen.userActed");
}

ActiveDirectoryPasswordChangeScreenHandler::
    ~ActiveDirectoryPasswordChangeScreenHandler() {}

void ActiveDirectoryPasswordChangeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adPassChangeMessage", IDS_AD_PASSWORD_CHANGE_MESSAGE);
}

void ActiveDirectoryPasswordChangeScreenHandler::Initialize() {}

void ActiveDirectoryPasswordChangeScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback("login.ActiveDirectoryPasswordChangeScreen.changePassword",
              &ActiveDirectoryPasswordChangeScreenHandler::HandleComplete);
}

void ActiveDirectoryPasswordChangeScreenHandler::Show(
    const std::string& username,
    int error) {
  base::DictionaryValue data;
  data.SetString(kUsernameKey, username);
  data.SetInteger(kErrorKey, error);
  ShowScreenWithData(kScreenId, &data);
}

void ActiveDirectoryPasswordChangeScreenHandler::Bind(
    ActiveDirectoryPasswordChangeScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ActiveDirectoryPasswordChangeScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ActiveDirectoryPasswordChangeScreenHandler::ShowSignInError(
    const std::string& error_text) {
  CallJS("login.ActiveDirectoryPasswordChangeScreen.showErrorDialog",
         error_text);
}

void ActiveDirectoryPasswordChangeScreenHandler::HandleComplete(
    const std::string& old_password,
    const std::string& new_password) {
  screen_->ChangePassword(old_password, new_password);
}

}  // namespace chromeos
