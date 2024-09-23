// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result.h"

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasswordStatusCheckResult::PasswordStatusCheckResult() = default;
PasswordStatusCheckResult::~PasswordStatusCheckResult() = default;

PasswordStatusCheckResult::PasswordStatusCheckResult(
    const PasswordStatusCheckResult&) = default;
PasswordStatusCheckResult& PasswordStatusCheckResult::operator=(
    const PasswordStatusCheckResult&) = default;

void PasswordStatusCheckResult::AddToCompromisedPasswords(
    std::string origin,
    std::string username) {
  compromised_passwords_.insert(PasswordPair(origin, username));
}

std::unique_ptr<SafetyHubService::Result> PasswordStatusCheckResult::Clone()
    const {
  return std::make_unique<PasswordStatusCheckResult>(*this);
}

base::Value::Dict PasswordStatusCheckResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List compromised_passwords;
  for (const PasswordPair& pair : compromised_passwords_) {
    base::Value::Dict password_data;
    password_data.Set(safety_hub::kOrigin, pair.origin);
    password_data.Set(safety_hub::kUsername, pair.username);
    compromised_passwords.Append(std::move(password_data));
  }
  result.Set(safety_hub::kSafetyHubPasswordCheckOriginsKey,
             std::move(compromised_passwords));
  return result;
}

bool PasswordStatusCheckResult::IsTriggerForMenuNotification() const {
  return !compromised_passwords_.empty();
}

bool PasswordStatusCheckResult::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::set<PasswordPair> old_passwords;
  for (const base::Value& password_data : *previous_result_dict.FindList(
           safety_hub::kSafetyHubPasswordCheckOriginsKey)) {
    // If the password_data is not a dict, this means it belongs to the previous
    // version of the stored data. Show the notification in this case.
    if (!password_data.is_dict()) {
      return true;
    }

    std::string origin =
        password_data.GetDict().Find(safety_hub::kOrigin)->GetString();
    std::string username =
        password_data.GetDict().Find(safety_hub::kUsername)->GetString();
    old_passwords.insert(PasswordPair(origin, username));
  }

  // Compare old and new set. If they are different, a notification should be
  // shown.
  const std::set<PasswordPair>& new_passwords = GetCompromisedPasswords();
  return new_passwords != old_passwords;
}

std::u16string PasswordStatusCheckResult::GetNotificationString() const {
  CHECK(!compromised_passwords_.empty());
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION,
      compromised_passwords_.size());
}

int PasswordStatusCheckResult::GetNotificationCommandId() const {
  return IDC_SAFETY_HUB_SHOW_PASSWORD_CHECKUP;
}
