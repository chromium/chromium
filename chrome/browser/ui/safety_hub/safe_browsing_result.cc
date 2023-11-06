// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/l10n/l10n_util.h"

SafetyHubSafeBrowsingResult::SafetyHubSafeBrowsingResult(
    SafeBrowsingState status)
    : status_(status) {}

SafetyHubSafeBrowsingResult::SafetyHubSafeBrowsingResult(
    const SafetyHubSafeBrowsingResult&) = default;
SafetyHubSafeBrowsingResult& SafetyHubSafeBrowsingResult::operator=(
    const SafetyHubSafeBrowsingResult&) = default;

SafetyHubSafeBrowsingResult::~SafetyHubSafeBrowsingResult() = default;

// static
absl::optional<std::unique_ptr<SafetyHubService::Result>>
SafetyHubSafeBrowsingResult::GetResult(const PrefService* pref_service) {
  if (safe_browsing::IsEnhancedProtectionEnabled(*pref_service)) {
    return std::make_unique<SafetyHubSafeBrowsingResult>(
        SafeBrowsingState::kEnabledEnhanced);
  }
  if (safe_browsing::IsSafeBrowsingEnabled(*pref_service)) {
    return std::make_unique<SafetyHubSafeBrowsingResult>(
        SafeBrowsingState::kEnabledStandard);
  }
  if (safe_browsing::IsSafeBrowsingPolicyManaged(*pref_service)) {
    return std::make_unique<SafetyHubSafeBrowsingResult>(
        SafeBrowsingState::kDisabledByAdmin);
  }
  if (safe_browsing::IsSafeBrowsingExtensionControlled(*pref_service)) {
    return std::make_unique<SafetyHubSafeBrowsingResult>(
        SafeBrowsingState::kDisabledByExtension);
  }
  return std::make_unique<SafetyHubSafeBrowsingResult>(
      SafeBrowsingState::kDisabledByUser);
}

SafetyHubSafeBrowsingResult::SafetyHubSafeBrowsingResult(
    const base::Value::Dict& dict) {
  status_ = static_cast<SafeBrowsingState>(
      dict.FindInt(safety_hub::kSafetyHubSafeBrowsingStatusKey).value());
}

std::unique_ptr<SafetyHubService::Result> SafetyHubSafeBrowsingResult::Clone()
    const {
  return std::make_unique<SafetyHubSafeBrowsingResult>(*this);
}

base::Value::Dict SafetyHubSafeBrowsingResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  result.Set(safety_hub::kSafetyHubSafeBrowsingStatusKey,
             static_cast<int>(status_));
  return result;
}

bool SafetyHubSafeBrowsingResult::IsTriggerForMenuNotification() const {
  return status_ == SafeBrowsingState::kDisabledByUser;
}

bool SafetyHubSafeBrowsingResult::WarrantsNewMenuNotification(
    const Result& previousResult) const {
  return true;
}

std::u16string SafetyHubSafeBrowsingResult::GetNotificationString() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MENU_NOTIFICATION);
}

int SafetyHubSafeBrowsingResult::GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}
