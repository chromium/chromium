// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"

#include <memory>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
namespace {
base::Value::Dict CardDataToValue(int header_id,
                                  int subheader_id,
                                  safety_hub::SafetyHubCardState card_state) {
  base::Value::Dict card_info;

  card_info.Set(safety_hub::kCardHeaderKey,
                l10n_util::GetStringUTF16(header_id));
  card_info.Set(safety_hub::kCardSubheaderKey,
                l10n_util::GetStringUTF16(subheader_id));
  card_info.Set(safety_hub::kCardStateKey, static_cast<int>(card_state));

  return card_info;
}
}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

SafetyHubSafeBrowsingResult::SafetyHubSafeBrowsingResult(
    SafeBrowsingState status)
    : status_(status) {}

SafetyHubSafeBrowsingResult::SafetyHubSafeBrowsingResult(
    const SafetyHubSafeBrowsingResult&) = default;
SafetyHubSafeBrowsingResult& SafetyHubSafeBrowsingResult::operator=(
    const SafetyHubSafeBrowsingResult&) = default;

SafetyHubSafeBrowsingResult::~SafetyHubSafeBrowsingResult() = default;

// static
std::optional<std::unique_ptr<SafetyHubResult>>
SafetyHubSafeBrowsingResult::GetResult(const PrefService* pref_service) {
  SafeBrowsingState state = SafetyHubSafeBrowsingResult::GetState(pref_service);
  return std::make_unique<SafetyHubSafeBrowsingResult>(state);
}

// static
SafeBrowsingState SafetyHubSafeBrowsingResult::GetState(
    const PrefService* pref_service) {
  if (safe_browsing::IsEnhancedProtectionEnabled(*pref_service)) {
    return SafeBrowsingState::kEnabledEnhanced;
  }
  if (safe_browsing::IsSafeBrowsingEnabled(*pref_service)) {
    return SafeBrowsingState::kEnabledStandard;
  }
  if (safe_browsing::IsSafeBrowsingPolicyManaged(*pref_service)) {
    return SafeBrowsingState::kDisabledByAdmin;
  }
  if (safe_browsing::IsSafeBrowsingExtensionControlled(*pref_service)) {
    return SafeBrowsingState::kDisabledByExtension;
  }
  return SafeBrowsingState::kDisabledByUser;
}

#if !BUILDFLAG(IS_ANDROID)
base::Value::Dict SafetyHubSafeBrowsingResult::GetSafeBrowsingCardData(
    const PrefService* pref_service) {
  base::Value::Dict sb_card_info;
  SafeBrowsingState state = SafetyHubSafeBrowsingResult::GetState(pref_service);
  switch (state) {
    case SafeBrowsingState::kEnabledEnhanced:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER,
                          safety_hub::SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kEnabledStandard:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER,
                          safety_hub::SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kDisabledByAdmin:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_MANAGED_SUBHEADER,
                          safety_hub::SafetyHubCardState::kInfo);
      break;
    case SafeBrowsingState::kDisabledByExtension:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_EXTENSION_SUBHEADER,
                          safety_hub::SafetyHubCardState::kInfo);
      break;
    default:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_USER_SUBHEADER,
                          safety_hub::SafetyHubCardState::kWarning);
  }
  return sb_card_info;
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::unique_ptr<SafetyHubResult> SafetyHubSafeBrowsingResult::Clone() const {
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
    const base::Value::Dict& previous_result_dict) const {
  return true;
}

std::u16string SafetyHubSafeBrowsingResult::GetNotificationString() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MENU_NOTIFICATION);
}

int SafetyHubSafeBrowsingResult::GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}
