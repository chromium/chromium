// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"

#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

OfflineLoginScreenHandler::OfflineLoginScreenHandler()
    : BaseScreenHandler(kScreenId) {}

OfflineLoginScreenHandler::~OfflineLoginScreenHandler() = default;

void OfflineLoginScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("offlineLoginEmail", IDS_OFFLINE_LOGIN_EMAIL);
  builder->Add("offlineLoginPassword", IDS_OFFLINE_LOGIN_PASSWORD);
  builder->Add("offlineLoginInvalidEmail", IDS_OFFLINE_LOGIN_INVALID_EMAIL);
  builder->Add("offlineLoginInvalidPassword",
               IDS_OFFLINE_LOGIN_INVALID_PASSWORD);
  builder->Add("offlineLoginNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordBtn",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordDlg",
               IDS_OFFLINE_LOGIN_FORGOT_PASSWORD_DIALOG_TEXT);
  builder->Add("offlineLoginCloseBtn", IDS_OFFLINE_LOGIN_CLOSE_BUTTON_TEXT);
  builder->Add("offlineLoginWarningTitle", IDS_OFFLINE_LOGIN_WARNING_TITLE);
  builder->Add("offlineLoginWarning", IDS_OFFLINE_LOGIN_WARNING_TEXT);
  builder->Add("offlineLoginOkBtn", IDS_OFFLINE_LOGIN_OK_BUTTON_TEXT);
}

void OfflineLoginScreenHandler::Show(base::Value::Dict params) {
  ShowInWebUI(std::move(params));
}

void OfflineLoginScreenHandler::Hide() {
  Reset();
}

void OfflineLoginScreenHandler::Reset() {
  CallExternalAPI("reset");
}

void OfflineLoginScreenHandler::ShowPasswordPage() {
  CallExternalAPI("proceedToPasswordPage");
}

void OfflineLoginScreenHandler::ShowOnlineRequiredDialog() {
  CallExternalAPI("showOnlineRequiredDialog");
}

void OfflineLoginScreenHandler::ShowPasswordMismatchMessage() {
  CallExternalAPI("showPasswordMismatchMessage");
}

base::WeakPtr<OfflineLoginView> OfflineLoginScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
