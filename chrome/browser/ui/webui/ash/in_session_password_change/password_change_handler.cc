// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_handler.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

PasswordChangeHandler::PasswordChangeHandler(
    const std::string& password_change_url)
    : password_change_url_(password_change_url) {}
PasswordChangeHandler::~PasswordChangeHandler() = default;

void PasswordChangeHandler::HandleInitialize(const base::Value::List& value) {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));

  AllowJavascript();
  base::Value::Dict params;
  if (password_change_url_.empty()) {
    LOG(ERROR) << "Password change url is empty";
    return;
  }
  params.Set("passwordChangeUrl", password_change_url_);
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    params.Set("userName", user->GetDisplayEmail());
  CallJavascriptFunction("$(\'main-element\').loadAuthenticator", params);
}

void PasswordChangeHandler::HandleChangePassword(
    const base::Value::List& params) {
  const base::Value& old_passwords = params[0];
  const base::Value& new_passwords = params[1];
  VLOG(4) << "Scraped " << old_passwords.GetList().size() << " old passwords";
  VLOG(4) << "Scraped " << new_passwords.GetList().size() << " new passwords";

  const std::string old_password = (old_passwords.GetList().size() > 0)
                                       ? old_passwords.GetList()[0].GetString()
                                       : "";
  const std::string new_password = (new_passwords.GetList().size() == 1)
                                       ? new_passwords.GetList()[0].GetString()
                                       : "";

  InSessionPasswordChangeManager::Get()->OnSamlPasswordChanged(old_password,
                                                               new_password);
}

void PasswordChangeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&PasswordChangeHandler::HandleInitialize,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "changePassword",
      base::BindRepeating(&PasswordChangeHandler::HandleChangePassword,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace ash
