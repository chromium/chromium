// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/key.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kUsernameKey[] = "username";
constexpr char kErrorKey[] = "error";

// Possible error states of the Active Directory password change screen. Must be
// in the same order as ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE enum
// values.
enum class ActiveDirectoryPasswordChangeErrorState {
  WRONG_OLD_PASSWORD = 0,
  NEW_PASSWORD_REJECTED = 1,
};

}  // namespace

ActiveDirectoryPasswordChangeScreenHandler::
    ActiveDirectoryPasswordChangeScreenHandler(
        JSCallsContainer* js_calls_container,
        CoreOobeView* core_oobe_view)
    : BaseScreenHandler(OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE,
                        js_calls_container),
      authpolicy_login_helper_(std::make_unique<AuthPolicyHelper>()),
      core_oobe_view_(core_oobe_view) {}

ActiveDirectoryPasswordChangeScreenHandler::
    ~ActiveDirectoryPasswordChangeScreenHandler() {}

void ActiveDirectoryPasswordChangeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("adPassChangeMessage", IDS_AD_PASSWORD_CHANGE_MESSAGE);
}

void ActiveDirectoryPasswordChangeScreenHandler::Initialize() {}

void ActiveDirectoryPasswordChangeScreenHandler::RegisterMessages() {
  AddCallback("completeActiveDirectoryPasswordChange",
              &ActiveDirectoryPasswordChangeScreenHandler::HandleComplete);
  AddCallback("cancelActiveDirectoryPasswordChange",
              &ActiveDirectoryPasswordChangeScreenHandler::HandleCancel);
}

void ActiveDirectoryPasswordChangeScreenHandler::HandleComplete(
    const std::string& username,
    const std::string& old_password,
    const std::string& new_password) {
  authpolicy_login_helper_->AuthenticateUser(
      username, std::string() /* object_guid */,
      old_password + "\n" + new_password + "\n" + new_password,
      base::BindOnce(
          &ActiveDirectoryPasswordChangeScreenHandler::OnAuthFinished,
          weak_factory_.GetWeakPtr(), username, Key(new_password)));
}

void ActiveDirectoryPasswordChangeScreenHandler::HandleCancel() {
  authpolicy_login_helper_->CancelRequestsAndRestart();
}

void ActiveDirectoryPasswordChangeScreenHandler::ShowScreen(
    const std::string& username) {
  base::DictionaryValue data;
  data.SetString(kUsernameKey, username);
  ShowScreenWithData(OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE,
                     &data);
}

void ActiveDirectoryPasswordChangeScreenHandler::ShowScreenWithError(
    int error) {
  base::DictionaryValue data;
  data.SetInteger(kErrorKey, error);
  ShowScreenWithData(OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE,
                     &data);
}

void ActiveDirectoryPasswordChangeScreenHandler::OnAuthFinished(
    const std::string& username,
    const Key& key,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  switch (error) {
    case authpolicy::ERROR_NONE: {
      DCHECK(account_info.has_account_id() &&
             !account_info.account_id().empty());
      const AccountId account_id = user_manager::known_user::GetAccountId(
          username, account_info.account_id(), AccountType::ACTIVE_DIRECTORY);
      DCHECK(LoginDisplayHost::default_host());
      LoginDisplayHost::default_host()->SetDisplayAndGivenName(
          account_info.display_name(), account_info.given_name());
      UserContext user_context(
          user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, account_id);
      user_context.SetKey(key);
      user_context.SetAuthFlow(UserContext::AUTH_FLOW_ACTIVE_DIRECTORY);
      user_context.SetIsUsingOAuth(false);
      LoginDisplayHost::default_host()->CompleteLogin(user_context);
      break;
    }
    case authpolicy::ERROR_BAD_PASSWORD:
      ShowScreenWithError(static_cast<int>(
          ActiveDirectoryPasswordChangeErrorState::WRONG_OLD_PASSWORD));
      break;
    case authpolicy::ERROR_PASSWORD_REJECTED:
      ShowScreenWithError(static_cast<int>(
          ActiveDirectoryPasswordChangeErrorState::NEW_PASSWORD_REJECTED));
      core_oobe_view_->ShowSignInError(
          0,
          l10n_util::GetStringUTF8(
              IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD_REJECTED_LONG_ERROR),
          std::string(), HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
      break;
    default:
      NOTREACHED() << "Unhandled error: " << error;
      ShowScreen(username);
      core_oobe_view_->ShowSignInError(
          0, l10n_util::GetStringUTF8(IDS_AD_AUTH_UNKNOWN_ERROR), std::string(),
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
  }
}

}  // namespace chromeos
