// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_enablement_service_impl.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/account_settings/account_setting_service.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace personal_context {
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
      &features::kPersonalContext,
      &features::kPersonalContextFirstRun,
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

// Checks whether miscellaneous "other" requirements (e.g. Geo-IP, locale)
// are satisfied.
[[nodiscard]] bool SatisfiesMiscellaneousRequirements(
    GeoIpCountryCode country_code,
    std::string_view locale,
    std::string* debug_message = nullptr) {
  if (country_code != GeoIpCountryCode("US")) {
    MaybeOutputReason(debug_message, "Unsupported GeoIp.");
    return false;
  }

  if (locale != "en-US") {
    MaybeOutputReason(debug_message, "Unsupported locale.");
    return false;
  }

  return true;
}

// Checks whether preference requirements are satisfied.
[[nodiscard]] PersonalContextEnablementState SatisfiesPreferenceRequirements(
    PrefService* pref_service,
    std::string* debug_message = nullptr) {
  using enum PersonalContextEnablementState;

  if (!pref_service) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return kDisabledNotEligible;
  }

  const bool notice_should_be_shown = pref_service->GetBoolean(
      prefs::kPersonalContextInAutofillNoticeShouldBeShown);
  const bool toggle_is_on = pref_service->GetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus);

  if (!toggle_is_on) {
    // The toggle is on-by-default. If it's off then it must have been disabled
    // by the user.
    MaybeOutputReason(debug_message, "User disabled via toggle.");
    return kDisabledViaPersonalIntelligenceInAutofillToggle;
  }

  if (notice_should_be_shown) {
    MaybeOutputReason(debug_message, "Notice not yet shown.");
    return kEnabledShouldShowNotice;
  }

  return kEnabled;
}
}  // namespace

PersonalContextEnablementServiceImpl::PersonalContextEnablementServiceImpl(
    account_settings::AccountSettingService* account_settings_service,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    GeoIpCountryCode country_code,
    std::string locale)
    : account_settings_service_(account_settings_service),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      country_code_(std::move(country_code)),
      locale_(std::move(locale)) {
  if (account_settings_service_) {
    account_settings_observation_.Observe(account_settings_service_);
  }
  if (identity_manager) {
    identity_manager_observer_.Observe(identity_manager);
  }
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        prefs::kPersonalContextInAutofillNoticeShouldBeShown,
        base::BindRepeating(
            &PersonalContextEnablementServiceImpl::UpdateEnablementState,
            base::Unretained(this)));
    pref_registrar_.Add(
        prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(
            &PersonalContextEnablementServiceImpl::UpdateEnablementState,
            base::Unretained(this)));
  }
  UpdateEnablementState();
}

PersonalContextEnablementServiceImpl::~PersonalContextEnablementServiceImpl() =
    default;

void PersonalContextEnablementServiceImpl::AddObserver(
    PersonalContextEnablementService::Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalContextEnablementServiceImpl::RemoveObserver(
    PersonalContextEnablementService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

PersonalContextEnablementState
PersonalContextEnablementServiceImpl::GetEnablementState() {
  if (base::FeatureList::IsEnabled(
          features::debug::kPersonalContextForceEnablementState)) {
    return static_cast<PersonalContextEnablementState>(
        features::debug::kPersonalContextForceEnablementStateParam.Get());
  }

  return enablement_state_;
}

PersonalContextEnablementState
PersonalContextEnablementServiceImpl::ComputeEnablementState() {
  using enum PersonalContextEnablementState;

  if (!SatisfiesFeatureRequirements()) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesAccountRequirements(identity_manager_.get())) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesMiscellaneousRequirements(country_code_, locale_)) {
    return kDisabledNotEligible;
  }

  if (!account_settings_service_) {
    return kDisabledNotEligible;
  }

  if (!SatisfiesOptInRequirements(account_settings_service_.get())) {
    return personal_context::features::IsPersonalContextFirstRunOptInEnabled()
               ? kDisabledNeedsOptIn
               : kDisabledNotEligible;
  }
  // SatisfiesPreferenceRequirements() needs to be called last: Up to this
  // point, general eligibility checks have been performed. Only if those are
  // satifsied, autofill specific prefs should be evaluated.
  return SatisfiesPreferenceRequirements(pref_service_.get());
}

void PersonalContextEnablementServiceImpl::UpdateEnablementState() {
  PersonalContextEnablementState new_state = ComputeEnablementState();
  if (new_state != enablement_state_) {
    enablement_state_ = new_state;
    observers_.Notify(
        &PersonalContextEnablementService::Observer::OnEnablementStateChanged,
        enablement_state_);
  }
}

void PersonalContextEnablementServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    if (pref_service_) {
      pref_service_->ClearPref(
          prefs::kPersonalContextInAutofillNoticeShouldBeShown);
      pref_service_->ClearPref(
          prefs::kPersonalContextInAutofillSettingsToggleStatus);
    }
  }
  UpdateEnablementState();
}

void PersonalContextEnablementServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer_.Reset();
}

void PersonalContextEnablementServiceImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateEnablementState();
}

void PersonalContextEnablementServiceImpl::OnAccountSettingDataUpdated(
    const std::string& setting_name) {
  UpdateEnablementState();
}

}  // namespace personal_context
