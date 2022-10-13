// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/in_session_password_change/confirm_password_change_handler.h"

#include <string>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

const InSessionPasswordChangeManager::Event kIncorrectPasswordEvent =
    InSessionPasswordChangeManager::Event::CRYPTOHOME_PASSWORD_CHANGE_FAILURE;

const InSessionPasswordChangeManager::PasswordSource kPasswordSource =
    InSessionPasswordChangeManager::PasswordSource::PASSWORDS_RETYPED;

// Returns one if it is non-empty, otherwise returns two.
const std::string& FirstNonEmpty(const std::string& one,
                                 const std::string& two) {
  return !one.empty() ? one : two;
}

}  // namespace

ConfirmPasswordChangeHandler::ConfirmPasswordChangeHandler(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password,
    const bool show_spinner_initially)
    : scraped_old_password_(scraped_old_password),
      scraped_new_password_(scraped_new_password),
      show_spinner_initially_(show_spinner_initially) {
  if (InSessionPasswordChangeManager::IsInitialized()) {
    InSessionPasswordChangeManager::Get()->AddObserver(this);
  }
}

ConfirmPasswordChangeHandler::~ConfirmPasswordChangeHandler() {
  if (InSessionPasswordChangeManager::IsInitialized()) {
    InSessionPasswordChangeManager::Get()->RemoveObserver(this);
  }
}

void ConfirmPasswordChangeHandler::HandleGetInitialState(
    const base::Value::List& params) {
  const std::string callback_id = params[0].GetString();

  base::Value::Dict state;
  state.Set("showOldPasswordPrompt", scraped_old_password_.empty());
  state.Set("showNewPasswordPrompt", scraped_new_password_.empty());
  state.Set("showSpinner", show_spinner_initially_);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, state);
}

void ConfirmPasswordChangeHandler::HandleChangePassword(
    const base::Value::List& params) {
  const std::string old_password =
      FirstNonEmpty(params[0].GetString(), scraped_old_password_);
  const std::string new_password =
      FirstNonEmpty(params[1].GetString(), scraped_new_password_);
  DCHECK(!old_password.empty() && !new_password.empty());

  InSessionPasswordChangeManager::Get()->ChangePassword(
      old_password, new_password, kPasswordSource);
}

void ConfirmPasswordChangeHandler::OnEvent(
    InSessionPasswordChangeManager::Event event) {
  if (event == kIncorrectPasswordEvent) {
    // If this event comes before getInitialState, then don't show the spinner
    // initially after all - the initial password change attempt using scraped
    // passwords already failed before the dialog finished loading.
    show_spinner_initially_ = false;
    // Discard the scraped old password and ask the user what it really is.
    scraped_old_password_.clear();
    if (IsJavascriptAllowed()) {
      FireWebUIListener("incorrect-old-password");
    }
  }
}

void ConfirmPasswordChangeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getInitialState",
      base::BindRepeating(&ConfirmPasswordChangeHandler::HandleGetInitialState,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "changePassword",
      base::BindRepeating(&ConfirmPasswordChangeHandler::HandleChangePassword,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace ash
