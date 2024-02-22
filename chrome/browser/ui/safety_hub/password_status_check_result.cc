// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result.h"

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasswordStatusCheckResult::PasswordStatusCheckResult() = default;
PasswordStatusCheckResult::~PasswordStatusCheckResult() = default;

PasswordStatusCheckResult::PasswordStatusCheckResult(
    const PasswordStatusCheckResult&) = default;
PasswordStatusCheckResult& PasswordStatusCheckResult::operator=(
    const PasswordStatusCheckResult&) = default;

void PasswordStatusCheckResult::AddToCompromisedOrigins(std::string origin) {
  compromised_origins_.insert(std::move(origin));
}

std::unique_ptr<SafetyHubService::Result> PasswordStatusCheckResult::Clone()
    const {
  return std::make_unique<PasswordStatusCheckResult>(*this);
}

base::Value::Dict PasswordStatusCheckResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List compromised_origins;
  for (const std::string& origin : compromised_origins_) {
    compromised_origins.Append(origin);
  }
  result.Set(kSafetyHubPasswordCheckOriginsKey, std::move(compromised_origins));
  return result;
}

bool PasswordStatusCheckResult::IsTriggerForMenuNotification() const {
  return !compromised_origins_.empty();
}

bool PasswordStatusCheckResult::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::set<std::string> old_origins;
  for (const base::Value& origin :
       *previous_result_dict.FindList(kSafetyHubPasswordCheckOriginsKey)) {
    old_origins.insert(origin.GetString());
  }
  const std::set<std::string>& new_origins = GetCompromisedOrigins();
  return !base::ranges::includes(old_origins, new_origins);
}

std::u16string PasswordStatusCheckResult::GetNotificationString() const {
  CHECK(!compromised_origins_.empty());
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION,
      compromised_origins_.size());
}

int PasswordStatusCheckResult::GetNotificationCommandId() const {
  return IDC_SHOW_PASSWORD_CHECKUP;
}
