// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_eligibility_service_impl.h"

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/account_settings/account_setting_service.h"
#if !BUILDFLAG(IS_IOS)
#include "components/glic/glic_pref_names.h"
#endif
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace personal_context {
namespace {

// Returns the forced enablement state as set via the feature parameter iff
// it corresponds to a valid enum entry and `std::nullopt` otherwise.
std::optional<PersonalContextEligibilityState> GetForcedEligibilityState() {
  const auto unsafe_type = static_cast<PersonalContextEligibilityState>(
      features::debug::kPersonalContextForceEnablementStateParam.Get());
  switch (unsafe_type) {
    case PersonalContextEligibilityState::kDisabledNotEligible:
    case PersonalContextEligibilityState::kEligible:
      return unsafe_type;
  }
  return std::nullopt;
}

// Helper function for debugging why a permissions check failed.
void MaybeOutputReason(std::string* out, std::string_view message) {
  if (out) {
    *out = std::string(message);
  }
}

void MaybeLogPersonalContextNonEligibility(
    std::optional<PersonalContextNonEligibilityReason> non_eligibility_reason) {
  if (!non_eligibility_reason) {
    return;
  }
  base::UmaHistogramEnumeration("Autofill.PersonalContext.NonEligibilityReason",
                                *non_eligibility_reason);
}

// Checks whether all requirements for `IdentityManager` state are met.
[[nodiscard]] std::pair<bool,
                        std::optional<PersonalContextNonEligibilityReason>>
SatisfiesAccountRequirements(const signin::IdentityManager* identity_manager,
                             std::string* debug_message = nullptr) {
  // The user is signed out.
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    MaybeOutputReason(debug_message, "User not signed into Chrome.");
    return std::pair{false, PersonalContextNonEligibilityReason::kNotSignedIn};
  }

  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin))) {
    MaybeOutputReason(debug_message,
                      "User's sign-in is in a persistent error state.");
    return std::pair{false, PersonalContextNonEligibilityReason::kNotSignedIn};
  }

  const AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfo(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));

  // Consumer account checks.
  if (extended_account_info.IsManaged() == signin::Tribool::kTrue) {
    MaybeOutputReason(debug_message, "The account is not a consumer account");
    return std::pair{false,
                     PersonalContextNonEligibilityReason::kNotConsumerAccount};
  }

  // TODO(crbug.com/494149753): This `can_use_model_execution_features()`
  // check is a very hacky way to check whether the user is underaged.
  // Consider defining a separate capability or syncing a separate setting
  // through ACCOUNT_SETTING instead.
  if (extended_account_info.GetAccountCapabilities()
          .can_use_model_execution_features() != signin::Tribool::kTrue) {
    MaybeOutputReason(debug_message, "User is underaged.");
    return std::pair{false,
                     PersonalContextNonEligibilityReason::kNotAgeEligible};
  }

  return std::pair{true, std::nullopt};
}

// Checks whether all requirements for `AccountSettingService` state are met.
[[nodiscard]] std::pair<bool,
                        std::optional<PersonalContextNonEligibilityReason>>
SatisfiesAccountSettingRequirements(
    account_settings::AccountSettingService* account_settings,
    std::string* debug_message = nullptr) {
  if (!account_settings) {
    MaybeOutputReason(debug_message, "Account settings service not available.");
    return std::pair{false, std::nullopt};
  }

  if (!account_settings->GetBoolean(account_settings::kAccountSettingContext)
           .value_or(false)) {
    MaybeOutputReason(debug_message, "Account is opted out of context");
    return std::pair{false,
                     PersonalContextNonEligibilityReason::kNotOptedInToContext};
  }

  if (!account_settings
           ->GetBoolean(account_settings::kAccountSettingContextWorkspace)
           .value_or(false) &&
      !account_settings
           ->GetBoolean(account_settings::kAccountSettingContextPhotos)
           .value_or(false)) {
    MaybeOutputReason(debug_message, "No context sources are enabled.");
    return std::pair{
        false,
        PersonalContextNonEligibilityReason::kNotPhotosAndWorkspaceAvailable};
  }
  return std::pair{true, std::nullopt};
}

// Checks whether all requirements for PrefService state are met.
[[nodiscard]] std::pair<bool,
                        std::optional<PersonalContextNonEligibilityReason>>
SatisfiesPrefsRequirements(const PrefService* pref_service,
                           std::string* debug_message = nullptr) {
  if (!pref_service) {
    MaybeOutputReason(debug_message, "Pref service not available.");
    return std::pair{false, std::nullopt};
  }

#if !BUILDFLAG(IS_IOS)
  const glic::prefs::FreStatus fre_status = static_cast<glic::prefs::FreStatus>(
      pref_service->GetInteger(::glic::prefs::kGlicCompletedFre));
  if (fre_status != glic::prefs::FreStatus::kCompleted) {
    MaybeOutputReason(debug_message, "GLIC FRE not completed.");
    return std::pair{false,
                     PersonalContextNonEligibilityReason::kNotGlicFirstRun};
  }
#endif

  const int policy_value = pref_service->GetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings);
  if (policy_value ==
      std::to_underlying(optimization_guide::model_execution::prefs::
                             ModelExecutionEnterprisePolicyValue::kDisable)) {
    MaybeOutputReason(
        debug_message,
        "Disallowed by FindAndFillWithGeminiSettings enterprise policy.");
    return std::pair{false, PersonalContextNonEligibilityReason::
                                kFindAndFillWithGeminiSettingsDisabled};
  }

  return std::pair{true, std::nullopt};
}

// Checks whether miscellaneous "other" requirements (e.g. Geo-IP, locale)
// are satisfied.
[[nodiscard]] std::pair<bool,
                        std::optional<PersonalContextNonEligibilityReason>>
SatisfiesMiscellaneousRequirements(GeoIpCountryCode country_code,
                                   std::string_view locale,
                                   std::string* debug_message = nullptr) {
  if (base::FeatureList::IsEnabled(
          features::debug::kAutofillAmbientAutofillSkipEligibilityChecks)) {
    return std::pair{true, std::nullopt};
  }

  if (country_code != GeoIpCountryCode("US")) {
    MaybeOutputReason(debug_message, "Unsupported GeoIp.");
    return std::pair{false, PersonalContextNonEligibilityReason::kNotGeoIpUS};
  }

  if (locale != "en-US") {
    MaybeOutputReason(debug_message, "Unsupported locale.");
    return std::pair{false,
                     PersonalContextNonEligibilityReason::kNotLocaleEnUS};
  }

  return std::pair{true, std::nullopt};
}
}  // namespace

PersonalContextEligibilityServiceImpl::PersonalContextEligibilityServiceImpl(
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
#if !BUILDFLAG(IS_IOS)
    pref_registrar_.Add(
        ::glic::prefs::kGlicCompletedFre,
        base::BindRepeating(
            &PersonalContextEligibilityServiceImpl::UpdateEligibilityState,
            base::Unretained(this)));
#endif
    pref_registrar_.Add(
        optimization_guide::prefs::kFindAndFillWithGeminiSettings,
        base::BindRepeating(
            &PersonalContextEligibilityServiceImpl::UpdateEligibilityState,
            base::Unretained(this)));
  }
  UpdateEligibilityState();
}

PersonalContextEligibilityServiceImpl::
    ~PersonalContextEligibilityServiceImpl() = default;

void PersonalContextEligibilityServiceImpl::AddObserver(
    PersonalContextEligibilityService::Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalContextEligibilityServiceImpl::RemoveObserver(
    PersonalContextEligibilityService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

PersonalContextEligibilityState
PersonalContextEligibilityServiceImpl::GetEligibilityState() {
  return eligibility_state_;
}

std::optional<PersonalContextNonEligibilityReason>
PersonalContextEligibilityServiceImpl::GetNonEligibilityReason() const {
  return last_non_eligibility_reason_;
}

std::pair<PersonalContextEligibilityState,
          std::optional<PersonalContextNonEligibilityReason>>
PersonalContextEligibilityServiceImpl::ComputeEligibilityState() {
  using enum PersonalContextEligibilityState;
  if (auto [satisfied, reason] =
          SatisfiesAccountRequirements(identity_manager_.get());
      !satisfied) {
    return std::pair{kDisabledNotEligible, reason};
  }

  if (base::FeatureList::IsEnabled(
          features::debug::kPersonalContextForceEnablementState)) {
    std::optional<PersonalContextEligibilityState> state =
        GetForcedEligibilityState();
    if (state) {
      return std::pair{state.value(),
                       PersonalContextNonEligibilityReason::kEligible};
    }
  }

  if (auto [satisfied, reason] =
          SatisfiesMiscellaneousRequirements(country_code_, locale_);
      !satisfied) {
    return std::pair{kDisabledNotEligible, reason};
  }

  if (!account_settings_service_) {
    return std::pair{kDisabledNotEligible, std::nullopt};
  }

  if (auto [satisfied, reason] =
          SatisfiesAccountSettingRequirements(account_settings_service_.get());
      !satisfied) {
    return std::pair{kDisabledNotEligible, reason};
  }

  if (auto [satisfied, reason] =
          SatisfiesPrefsRequirements(pref_service_.get());
      !satisfied) {
    return std::pair{kDisabledNotEligible, reason};
  }

  return std::pair{kEligible, PersonalContextNonEligibilityReason::kEligible};
}

void PersonalContextEligibilityServiceImpl::UpdateEligibilityState() {
  const auto [new_eligibility_state, non_eligibility_reason] =
      ComputeEligibilityState();
  if (new_eligibility_state != eligibility_state_) {
    eligibility_state_ = new_eligibility_state;
    observers_.Notify(
        &PersonalContextEligibilityService::Observer::OnEligibilityStateChanged,
        eligibility_state_);
  }
  if (non_eligibility_reason != last_non_eligibility_reason_) {
    last_non_eligibility_reason_ = non_eligibility_reason;
    if (base::FeatureList::IsEnabled(
            personal_context::features::kPersonalContextLogNonEligibilityUma)) {
      MaybeLogPersonalContextNonEligibility(non_eligibility_reason);
    }
  }
}

void PersonalContextEligibilityServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdateEligibilityState();
}

void PersonalContextEligibilityServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer_.Reset();
}

void PersonalContextEligibilityServiceImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateEligibilityState();
}

void PersonalContextEligibilityServiceImpl::OnAccountSettingDataUpdated(
    const std::string& setting_name) {
  UpdateEligibilityState();
}

void PersonalContextEligibilityServiceImpl::OnAccountSettingsLoaded() {
  UpdateEligibilityState();
}

}  // namespace personal_context
