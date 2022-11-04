// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/active_directory_password_change_screen_handler.h"

#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

namespace {

constexpr char kUsernameKey[] = "username";
constexpr char kErrorKey[] = "error";

}  // namespace

ActiveDirectoryPasswordChangeScreenHandler::
    ActiveDirectoryPasswordChangeScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ActiveDirectoryPasswordChangeScreenHandler::
    ~ActiveDirectoryPasswordChangeScreenHandler() = default;

void ActiveDirectoryPasswordChangeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adPassChangeMessage", IDS_AD_PASSWORD_CHANGE_MESSAGE);
}

void ActiveDirectoryPasswordChangeScreenHandler::Show(
    const std::string& username,
    int error) {
  base::Value::Dict data;
  data.Set(kUsernameKey, username);
  data.Set(kErrorKey, error);
  ShowInWebUI(std::move(data));
}

void ActiveDirectoryPasswordChangeScreenHandler::ShowSignInError(
    const std::string& error_text) {
  CallExternalAPI("showErrorDialog", error_text);
}

}  // namespace ash
