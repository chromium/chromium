// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/in_session_password_change/urgent_password_expiry_notification_handler.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/saml/password_expiry_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

UrgentPasswordExpiryNotificationHandler::
    UrgentPasswordExpiryNotificationHandler() = default;

UrgentPasswordExpiryNotificationHandler::
    ~UrgentPasswordExpiryNotificationHandler() = default;

void UrgentPasswordExpiryNotificationHandler::HandleContinue(
    const base::Value::List& params) {
  InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
}

void UrgentPasswordExpiryNotificationHandler::HandleGetTitleText(
    const base::Value::List& params) {
  const std::string callback_id = params[0].GetString();
  const int ms_until_expiry = params[1].GetInt();

  const std::u16string title = PasswordExpiryNotification::GetTitleText(
      base::Milliseconds(ms_until_expiry));

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::UTF16ToUTF8(title));
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

}  // namespace ash
