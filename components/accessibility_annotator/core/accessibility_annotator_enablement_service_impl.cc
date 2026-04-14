// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/accessibility_annotator_debug_features.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/country_type.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/account_settings/account_setting_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

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

const base::flat_set<int32_t>& GetAnnotatorEligibleTiers() {
  static const base::NoDestructor<base::flat_set<int32_t>> eligible_tiers([] {
    std::string tier_list =
        features::kAccessibilityAnnotatorEligibleTiers.Get();
    std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
        tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base::flat_set<int32_t> tiers;
    tiers.reserve(tier_pieces.size());
    for (std::string_view piece : tier_pieces) {
      int32_t tier_id = 0;
      if (base::StringToInt(piece, &tier_id)) {
        tiers.insert(tier_id);
      }
    }
    return tiers;
  }());
  return *eligible_tiers;
}

// Checks whether all requirements for `IdentityManager` state are met.
[[nodiscard]] bool SatisfiesAccountRequirements(
    const signin::IdentityManager* identity_manager,
    subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
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

  const AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfo(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));

  // Consumer account checks.
  if (extended_account_info.IsManaged() == signin::Tribool::kTrue) {
    MaybeOutputReason(debug_message, "The account is not a consumer account");
    return false;
  }

  // TODO(crbug.com/494149753): This `can_use_model_execution_features()`
  // check is a very hacky way to check whether the user is underaged.
  // Consider defining a separate capability or syncing a separate setting
  // through ACCOUNT_SETTING instead.
  if (extended_account_info.capabilities.can_use_model_execution_features() !=
      signin::Tribool::kTrue) {
    MaybeOutputReason(debug_message, "User is underaged.");
    return false;
  }

  if (!subscription_eligibility_service) {
    MaybeOutputReason(debug_message,
                      "Subscription eligibility service not available.");
    return false;
  }

  const int32_t tier =
      subscription_eligibility_service->GetAiSubscriptionTier();
  if (!GetAnnotatorEligibleTiers().contains(tier)) {
    MaybeOutputReason(debug_message, "User subscription tier is not eligible.");
    return false;
  }

  return true;
}

// Checks whether all opt-in for `AccountSettingService` state are met.
[[nodiscard]] bool SatisfiesOptInRequirements(
    account_settings::AccountSettingService* account_settings,
    std::string* debug_message = nullptr) {
  if (!account_settings) {
    MaybeOutputReason(debug_message, "Account settings service not available.");
    return false;
  }

  if (!account_settings->GetBoolean(account_settings::kAccountSettingContext)
           .value_or(false)) {
    MaybeOutputReason(debug_message, "Account is opted out of context");
    return false;
  }

  if (!account_settings
           ->GetBoolean(account_settings::kAccountSettingContextWorkspace)
           .value_or(false) &&
      !account_settings
           ->GetBoolean(account_settings::kAccountSettingContextPhotos)
           .value_or(false)) {
    MaybeOutputReason(debug_message, "No context sources are enabled.");
    return false;
  }
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

  if (pref_service->GetBoolean(prefs::kShouldShowRemoteAnnotatorFirstRunInfo)) {
    MaybeOutputReason(debug_message, "Info not yet acknowledged.");
    return kDisabledPendingInfo;
  }

  return kEnabled;
}
}  // namespace

AccessibilityAnnotatorEnablementServiceImpl::
    AccessibilityAnnotatorEnablementServiceImpl(
        account_settings::AccountSettingService* account_settings_service,
        signin::IdentityManager* identity_manager,
        subscription_eligibility::SubscriptionEligibilityService*
            subscription_eligibility_service,
        PrefService* pref_service,
        GeoIpCountryCode country_code)
    : account_settings_service_(account_settings_service),
      identity_manager_(identity_manager),
      subscription_eligibility_service_(subscription_eligibility_service),
      pref_service_(pref_service),
      country_code_(std::move(country_code)) {
  if (identity_manager) {
    identity_manager_observer_.Observe(identity_manager);
  }
  if (subscription_eligibility_service_) {
    subscription_eligibility_observer_.Observe(
        subscription_eligibility_service_);
  }
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
        base::BindRepeating(
            &AccessibilityAnnotatorEnablementServiceImpl::UpdateEnablementState,
            base::Unretained(this)));
  }
  UpdateEnablementState();
}

AccessibilityAnnotatorEnablementServiceImpl::
    ~AccessibilityAnnotatorEnablementServiceImpl() = default;

void AccessibilityAnnotatorEnablementServiceImpl::AddObserver(
    AccessibilityAnnotatorEnablementService::Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorEnablementServiceImpl::RemoveObserver(
    AccessibilityAnnotatorEnablementService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

RemoteAnnotatorEnablementState
AccessibilityAnnotatorEnablementServiceImpl::GetEnablementState() {
  if (base::FeatureList::IsEnabled(
          features::debug::kAccessibilityAnnotatorForceEnablementState)) {
    return static_cast<RemoteAnnotatorEnablementState>(
        features::debug::kAccessibilityAnnotatorForceEnablementStateParam
            .Get());
  }

  return enablement_state_;
}

RemoteAnnotatorEnablementState
AccessibilityAnnotatorEnablementServiceImpl::ComputeEnablementState() {
  using enum RemoteAnnotatorEnablementState;

  if (!SatisfiesFeatureRequirements()) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesAccountRequirements(identity_manager_.get(),
                                    subscription_eligibility_service_.get())) {
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

void AccessibilityAnnotatorEnablementServiceImpl::UpdateEnablementState() {
  RemoteAnnotatorEnablementState new_state = ComputeEnablementState();
  if (new_state != enablement_state_) {
    enablement_state_ = new_state;
    observers_.Notify(&AccessibilityAnnotatorEnablementService::Observer::
                          OnEnablementStateChanged,
                      enablement_state_);
  }
}

void AccessibilityAnnotatorEnablementServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    if (pref_service_) {
      pref_service_->ClearPref(prefs::kShouldShowRemoteAnnotatorFirstRunInfo);
    }
  }
  UpdateEnablementState();
}

void AccessibilityAnnotatorEnablementServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer_.Reset();
}

void AccessibilityAnnotatorEnablementServiceImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateEnablementState();
}

void AccessibilityAnnotatorEnablementServiceImpl::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  UpdateEnablementState();
}

}  // namespace accessibility_annotator
