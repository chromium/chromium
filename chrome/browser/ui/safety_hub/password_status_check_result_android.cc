// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result_android.h"

#include <optional>

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasswordStatusCheckResultAndroid::PasswordStatusCheckResultAndroid() = default;
PasswordStatusCheckResultAndroid::~PasswordStatusCheckResultAndroid() = default;

PasswordStatusCheckResultAndroid::PasswordStatusCheckResultAndroid(
    const PasswordStatusCheckResultAndroid&) = default;
PasswordStatusCheckResultAndroid& PasswordStatusCheckResultAndroid::operator=(
    const PasswordStatusCheckResultAndroid&) = default;

void PasswordStatusCheckResultAndroid::UpdateCompromisedPasswordCount(
    int count) {
  compromised_passwords_count_ = count;
}

std::unique_ptr<SafetyHubService::Result>
PasswordStatusCheckResultAndroid::Clone() const {
  return std::make_unique<PasswordStatusCheckResultAndroid>(*this);
}

base::Value::Dict PasswordStatusCheckResultAndroid::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List compromised_passwords;
  result.Set(safety_hub::kSafetyHubCompromiedPasswordOriginsCount,
             base::Value(compromised_passwords_count_));
  return result;
}

bool PasswordStatusCheckResultAndroid::IsTriggerForMenuNotification() const {
  return compromised_passwords_count_ > 0;
}

bool PasswordStatusCheckResultAndroid::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::optional<int> prev_count = previous_result_dict.FindInt(
      safety_hub::kSafetyHubCompromiedPasswordOriginsCount);
  if (!prev_count.has_value()) {
    return false;
  }

  // Compare old and new set. If they are different, a notification should be
  // shown.
  return compromised_passwords_count_ > prev_count.value();
}

std::u16string PasswordStatusCheckResultAndroid::GetNotificationString() const {
  CHECK(compromised_passwords_count_ > 0);
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION,
      compromised_passwords_count_);
}

int PasswordStatusCheckResultAndroid::GetNotificationCommandId() const {
  // Command is is not used in Clank, so returning -1 .
  return -1;
}
