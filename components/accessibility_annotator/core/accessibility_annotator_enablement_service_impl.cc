// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/accessibility_annotator_debug_features.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/country_type.h"
#include "components/account_settings/account_setting_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace accessibility_annotator {
namespace {
// Helper function for debugging why a permissions check failed.
void MaybeOutputReason(std::string* out, std::string_view message) {
  if (out) {
    *out = std::string(message);
  }
}

// Checks whether all requirements for `base::Feature` state are satisfied.
[[nodiscard]] bool SatisfiesFeatureRequirements(
    std::string* debug_message = nullptr) {
  const base::Feature* const kRequiredFeatures[] = {
      &features::kAccessibilityAnnotator,
      &features::kAccessibilityAnnotatorFirstRun,
      &features::kAccessibilityAnnotatorDatabaseStorage,
  };

  for (const base::Feature* feature : kRequiredFeatures) {
    if (!base::FeatureList::IsEnabled(*feature)) {
      MaybeOutputReason(debug_message,
                        base::StrCat({feature->name, " is not enabled."}));
      return false;
    }
  }

  return true;
}

// Checks whether all requirements for `IdentityManager` state are met.
[[nodiscard]] bool SatisfiesAccountRequirements(
    const signin::IdentityManager* identity_manager,
    std::string* debug_message = nullptr) {
  // The user is signed out.
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    MaybeOutputReason(debug_message, "User not signed into Chrome.");
    return false;
  }

  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin))) {
    MaybeOutputReason(debug_message,
                      "User's sign-in is in a persistent error state.");
    return false;
  }

  // TODO(crbug.com/494149753): This `can_use_model_execution_features()`
  // check is a very hacky way to check whether the user is underaged.
  // Consider defining a separate capability or syncing a separate setting
  // through ACCOUNT_SETTING instead.
  if (identity_manager
          ->FindExtendedAccountInfo(identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities.can_use_model_execution_features() !=
      signin::Tribool::kTrue) {
    MaybeOutputReason(debug_message, "User is underaged.");
    return false;
  }

  return true;
}

// Checks whether all opt-in for `AccountSettingService` state are met.
[[nodiscard]] bool SatisfiesOptInRequirements(
    account_settings::AccountSettingService* account_settings) {
  // TODO(crbug.com/494149753) Implement
  return true;
}

// Checks whether miscellaneous "other" requirements (e.g. Geo-IP)
// are satisfied.
[[nodiscard]] bool SatisfiesMiscellaneousRequirements(
    GeoIpCountryCode country_code,
    std::string* debug_message = nullptr) {
  if (country_code != GeoIpCountryCode("US")) {
    MaybeOutputReason(debug_message, "Unsupported GeoIp.");
    return false;
  }

  return true;
}

// Checks whether preference requirements are satisfied.
[[nodiscard]] RemoteAnnotatorEnablementState SatisfiesPreferenceRequirements(
    PrefService* pref_service,
    std::string* debug_message = nullptr) {
  using enum RemoteAnnotatorEnablementState;

  if (!pref_service) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return kDisabledNotEligible;
  }
  // TODO(crbug.com/494149753): Implement preference checks.
  return kEnabled;
}
}  // namespace

AccessibilityAnnotatorEnablementServiceImpl::
    AccessibilityAnnotatorEnablementServiceImpl(
        account_settings::AccountSettingService* account_settings_service,
        signin::IdentityManager* identity_manager,
        PrefService* pref_service,
        GeoIpCountryCode country_code)
    : account_settings_service_(account_settings_service),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      country_code_(std::move(country_code)) {}

AccessibilityAnnotatorEnablementServiceImpl::
    ~AccessibilityAnnotatorEnablementServiceImpl() = default;

void AccessibilityAnnotatorEnablementServiceImpl::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorEnablementServiceImpl::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

RemoteAnnotatorEnablementState
AccessibilityAnnotatorEnablementServiceImpl::GetEnablementState() {
  using enum RemoteAnnotatorEnablementState;
  if (base::FeatureList::IsEnabled(
          features::debug::kAccessibilityAnnotatorForceEnablementState)) {
    return static_cast<RemoteAnnotatorEnablementState>(
        features::debug::kAccessibilityAnnotatorForceEnablementStateParam
            .Get());
  }

  if (!SatisfiesFeatureRequirements()) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesAccountRequirements(identity_manager_.get())) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesOptInRequirements(account_settings_service_.get())) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesMiscellaneousRequirements(country_code_)) {
    return kDisabledNotEligible;
  }

  return SatisfiesPreferenceRequirements(pref_service_.get());
}

}  // namespace accessibility_annotator
