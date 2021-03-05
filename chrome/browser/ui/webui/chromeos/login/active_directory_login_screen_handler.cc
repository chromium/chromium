// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/active_directory_login_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

constexpr StaticOobeScreenId ActiveDirectoryLoginView::kScreenId;

ActiveDirectoryLoginScreenHandler::ActiveDirectoryLoginScreenHandler(
    JSCallsContainer* js_calls_container,
    CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
  set_user_acted_method_path("login.ActiveDirectoryLoginScreen.userActed");
}

ActiveDirectoryLoginScreenHandler::~ActiveDirectoryLoginScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ActiveDirectoryLoginScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback("completeAdAuthentication",
              &ActiveDirectoryLoginScreenHandler::HandleCompleteAuth);
}

void ActiveDirectoryLoginScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adAuthWelcomeMessage", IDS_AD_DOMAIN_AUTH_WELCOME_MESSAGE);
  builder->Add("adAuthLoginUsername", IDS_AD_AUTH_LOGIN_USER);
  builder->Add("adLoginPassword", IDS_AD_LOGIN_PASSWORD);
  builder->AddF("loginWelcomeMessage", IDS_LOGIN_WELCOME_MESSAGE,
                ui::GetChromeOSDeviceTypeResourceId());
}

void ActiveDirectoryLoginScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void ActiveDirectoryLoginScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  base::DictionaryValue screen_data;
  screen_data.SetString("realm", g_browser_process->platform_part()
                                     ->browser_policy_connector_chromeos()
                                     ->GetRealm());
  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    screen_data.SetString("emailDomain", email_domain);
  }

  ShowScreenWithData(kScreenId, &screen_data);
}

void ActiveDirectoryLoginScreenHandler::Bind(
    ActiveDirectoryLoginScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ActiveDirectoryLoginScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ActiveDirectoryLoginScreenHandler::Reset() {
  CallJS("login.ActiveDirectoryLoginScreen.reset");
}

void ActiveDirectoryLoginScreenHandler::SetErrorState(
    const std::string& username,
    int errorState) {
  CallJS("login.ActiveDirectoryLoginScreen.setErrorState", username,
         errorState);
}

void ActiveDirectoryLoginScreenHandler::ShowSignInError(
    const std::string& error_text) {
  core_oobe_view_->ShowSignInError(0 /* login_attempts */, error_text,
                                   std::string() /* help_link_text */,
                                   HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void ActiveDirectoryLoginScreenHandler::HandleCompleteAuth(
    const std::string& username,
    const std::string& password) {
  screen_->HandleCompleteAuth(username, password);
}

}  // namespace chromeos
