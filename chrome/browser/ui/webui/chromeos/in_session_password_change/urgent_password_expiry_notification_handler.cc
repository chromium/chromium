// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/urgent_password_expiry_notification_handler.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/chromeos/login/saml/password_expiry_notification.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

UrgentPasswordExpiryNotificationHandler::
    UrgentPasswordExpiryNotificationHandler() = default;

UrgentPasswordExpiryNotificationHandler::
    ~UrgentPasswordExpiryNotificationHandler() = default;

void UrgentPasswordExpiryNotificationHandler::HandleContinue(
    const base::ListValue* params) {
  InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
}

void UrgentPasswordExpiryNotificationHandler::HandleGetTitleText(
    const base::ListValue* params) {
  const std::string callback_id = params->GetList()[0].GetString();
  const int ms_until_expiry = params->GetList()[1].GetInt();

  const base::string16 title = PasswordExpiryNotification::GetTitleText(
      base::TimeDelta::FromMilliseconds(ms_until_expiry));

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(title));
}

void UrgentPasswordExpiryNotificationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "continue", base::BindRepeating(
                      &UrgentPasswordExpiryNotificationHandler::HandleContinue,
                      weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getTitleText",
      base::BindRepeating(
          &UrgentPasswordExpiryNotificationHandler::HandleGetTitleText,
          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
