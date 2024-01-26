// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"

namespace safe_browsing {

namespace {

using ProtectionLevel = SafeBrowsingPolicyHandler::ProtectionLevel;

// The result of checking a policy value.
enum class PolicyCheckResult {
  // The policy is not set.
  kNotSet,
  // The policy is set to an invalid value.
  kInvalid,
  // The policy is set to a valid value.
  kValid
};

// Checks the value of the SafeBrowsingEnabled policy. |errors| may be
// nullptr.
PolicyCheckResult CheckSafeBrowsingEnabled(
    const base::Value* safe_browsing_enabled,
    policy::PolicyErrorMap* errors) {
  if (!safe_browsing_enabled)
    return PolicyCheckResult::kNotSet;

  if (!safe_browsing_enabled->is_bool()) {
    if (errors) {
      errors->AddError(policy::key::kSafeBrowsingEnabled, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::BOOLEAN));
    }
    return PolicyCheckResult::kInvalid;
  }

  return PolicyCheckResult::kValid;
}

// Returns the target value of the Safe Browsing Protection Level derived only
// from the legacy SafeBrowsingEnabled policy. If this policy is not set or
// does not have a valid value, returns |nullopt|.
std::optional<ProtectionLevel> GetValueFromSafeBrowsingEnabledPolicy(
    const policy::PolicyMap& policies) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* safe_browsing_enabled =
      policies.GetValueUnsafe(policy::key::kSafeBrowsingEnabled);

  if (CheckSafeBrowsingEnabled(safe_browsing_enabled, nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return std::nullopt;
  }

  return safe_browsing_enabled->GetBool() ? ProtectionLevel::kStandardProtection
                                          : ProtectionLevel::kNoProtection;
}

// Returns true if |value| is within the valid range of the
// SafeBrowsingProtectionLevel enum policy.
bool IsValidSafeBrowsingProtectionLevelValue(int value) {
  return value >= 0 && value <= static_cast<int>(ProtectionLevel::kMaxValue);
}

// Checks the value of the SafeBrowsingProtectionLevel policy. |errors| may be
// nullptr.
PolicyCheckResult CheckSafeBrowsingProtectionLevel(
    const base::Value* safe_browsing_protection_level,
    policy::PolicyErrorMap* errors) {
  if (!safe_browsing_protection_level)
    return PolicyCheckResult::kNotSet;

  if (!safe_browsing_protection_level->is_int()) {
    if (errors) {
      errors->AddError(policy::key::kSafeBrowsingProtectionLevel,
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
    }
    return PolicyCheckResult::kInvalid;
  }

  const int value = safe_browsing_protection_level->GetInt();
  if (!IsValidSafeBrowsingProtectionLevelValue(value)) {
    if (errors) {
      errors->AddError(policy::key::kSafeBrowsingProtectionLevel,
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(value));
    }
    return PolicyCheckResult::kInvalid;
  }
  return PolicyCheckResult::kValid;
}

// Returns the target value of Safe Browsing protection level derived only
// from the SafeBrowsingProtectionLevel policy. If this policy is not set or
// does not have a valid value, returns |nullopt|.
std::optional<ProtectionLevel> GetValueFromSafeBrowsingProtectionLevelPolicy(
    const policy::PolicyMap& policies) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* safe_browsing_protection_level =
      policies.GetValueUnsafe(policy::key::kSafeBrowsingProtectionLevel);

  if (CheckSafeBrowsingProtectionLevel(safe_browsing_protection_level,
                                       nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return std::nullopt;
  }

  return static_cast<ProtectionLevel>(safe_browsing_protection_level->GetInt());
}

// Returns the target value of Safe Browsing protection level, derived from
// both the SafeBrowsingEnabled policy and the
// SafeBrowsingProtectionLevel policy. If both policies are set,
// SafeBrowsingProtectionLevel wins.
std::optional<ProtectionLevel> GetValueFromBothPolicies(
    const policy::PolicyMap& policies) {
  const std::optional<ProtectionLevel> safe_browsing_protection_level =
      GetValueFromSafeBrowsingProtectionLevelPolicy(policies);

  if (safe_browsing_protection_level.has_value()) {
    // SafeBrowsingProtectionLevel overrides SafeBrowsingEnabled policy.
    return safe_browsing_protection_level;
  }

  return GetValueFromSafeBrowsingEnabledPolicy(policies);
}

}  // namespace

bool SafeBrowsingPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // Deprecated boolean policy SafeBrowsingEnabled.
  const base::Value* safe_browsing_enabled =
      policies.GetValueUnsafe(policy::key::kSafeBrowsingEnabled);
  PolicyCheckResult safe_browsing_enabled_result =
      CheckSafeBrowsingEnabled(safe_browsing_enabled, errors);

  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // Enumerated policy SafeBrowsingProtectionLevel.
  const base::Value* safe_browsing_protection_level =
      policies.GetValueUnsafe(policy::key::kSafeBrowsingProtectionLevel);
  PolicyCheckResult safe_browsing_protection_level_result =
      CheckSafeBrowsingProtectionLevel(safe_browsing_protection_level, errors);

  if (safe_browsing_enabled_result == PolicyCheckResult::kValid &&
      safe_browsing_protection_level_result == PolicyCheckResult::kValid) {
    errors->AddError(policy::key::kSafeBrowsingEnabled, IDS_POLICY_OVERRIDDEN,
                     policy::key::kSafeBrowsingProtectionLevel);
  }

  // Always continue to ApplyPolicySettings which can handle invalid policy
  // values.
  return true;
}

void SafeBrowsingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const std::optional<ProtectionLevel> value =
      GetValueFromBothPolicies(policies);

  if (!value.has_value())
    return;

  switch (value.value()) {
    case ProtectionLevel::kNoProtection:
      prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);
      prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
      return;
    case ProtectionLevel::kStandardProtection:
      prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
      prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
      return;
    case ProtectionLevel::kEnhancedProtection:
      // |kSafeBrowsingEnhanced| is enabled, but so is
      // |kSafeBrowsingEnabled| because the extensions API should see Safe
      // Browsing as enabled. See https://crbug.com/1064722 for more background.
      prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
      prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
      return;
  }
}

// static
SafeBrowsingPolicyHandler::ProtectionLevel
SafeBrowsingPolicyHandler::GetSafeBrowsingProtectionLevel(
    const PrefService* pref_sevice) {
  bool safe_browsing_enhanced =
      pref_sevice->GetBoolean(prefs::kSafeBrowsingEnhanced);
  bool safe_browsing_enabled =
      pref_sevice->GetBoolean(prefs::kSafeBrowsingEnabled);

  if (safe_browsing_enhanced)
    return ProtectionLevel::kEnhancedProtection;

  if (safe_browsing_enabled)
    return ProtectionLevel::kStandardProtection;

  return ProtectionLevel::kNoProtection;
}

// static
bool SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
    const PrefService* pref_service) {
  bool is_safe_browsing_enabled_managed =
      pref_service->IsManagedPreference(prefs::kSafeBrowsingEnabled);
  bool is_safe_browsing_enhanced_managed =
      pref_service->IsManagedPreference(prefs::kSafeBrowsingEnhanced);
  DCHECK_EQ(is_safe_browsing_enabled_managed,
            is_safe_browsing_enhanced_managed);
  return is_safe_browsing_enabled_managed && is_safe_browsing_enhanced_managed;
}

}  // namespace safe_browsing
