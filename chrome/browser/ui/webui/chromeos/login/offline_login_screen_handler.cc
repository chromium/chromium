// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"

#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

constexpr StaticOobeScreenId OfflineLoginView::kScreenId;

OfflineLoginScreenHandler::OfflineLoginScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.OfflineLoginScreen.userActed");
}

OfflineLoginScreenHandler::~OfflineLoginScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void OfflineLoginScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback("completeOfflineAuthentication",
              &OfflineLoginScreenHandler::HandleCompleteAuth);
  AddCallback("OfflineLogin.onEmailSubmitted",
              &OfflineLoginScreenHandler::HandleEmailSubmitted);
}

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

void OfflineLoginScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void OfflineLoginScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OfflineLoginView::kScreenId);
}

void OfflineLoginScreenHandler::Hide() {
  Reset();
}

void OfflineLoginScreenHandler::Bind(OfflineLoginScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void OfflineLoginScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void OfflineLoginScreenHandler::Reset() {
  CallJS("login.OfflineLoginScreen.reset");
}

void OfflineLoginScreenHandler::HandleCompleteAuth(
    const std::string& username,
    const std::string& password) {
  screen_->HandleCompleteAuth(username, password);
}

void OfflineLoginScreenHandler::HandleEmailSubmitted(
    const std::string& username) {
  screen_->HandleEmailSubmitted(username);
}

void OfflineLoginScreenHandler::LoadParams(base::DictionaryValue& params) {
  CallJS("login.OfflineLoginScreen.loadParams", params);
}

void OfflineLoginScreenHandler::ShowPasswordPage() {
  CallJS("login.OfflineLoginScreen.proceedToPasswordPage");
}

void OfflineLoginScreenHandler::ShowOnlineRequiredDialog() {
  CallJS("login.OfflineLoginScreen.showOnlineRequiredDialog");
}

}  // namespace chromeos
