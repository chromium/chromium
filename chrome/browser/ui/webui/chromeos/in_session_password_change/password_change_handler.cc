// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_handler.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

PasswordChangeHandler::PasswordChangeHandler(
    const std::string& password_change_url)
    : password_change_url_(password_change_url) {}
PasswordChangeHandler::~PasswordChangeHandler() = default;

void PasswordChangeHandler::HandleInitialize(const base::ListValue* value) {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));

  AllowJavascript();
  base::Value params(base::Value::Type::DICTIONARY);
  if (password_change_url_.empty()) {
    LOG(ERROR) << "Password change url is empty";
    return;
  }
  params.SetKey("passwordChangeUrl", base::Value(password_change_url_));
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    params.SetKey("userName", base::Value(user->GetDisplayEmail()));
  CallJavascriptFunction("insession.password.change.loadAuthExtension", params);
}

void PasswordChangeHandler::HandleChangePassword(
    const base::ListValue* params) {
  const base::Value& old_passwords = params->GetList()[0];
  const base::Value& new_passwords = params->GetList()[1];
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

}  // namespace chromeos
