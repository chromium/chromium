// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/service/google_groups_manager.h"  // nogncheck
#endif

namespace autofill {

namespace {

[[nodiscard]] bool IsPersonalContextEligible(
    personal_context::PersonalContextEnablementService*
        personal_context_service) {
  if (!personal_context_service) {
    return false;
  }
  using enum personal_context::PersonalContextEligibilityState;
  switch (personal_context_service->GetEligibilityState()) {
    case kDisabledNotEligible:
    // TODO(crbug.com/504893949) Consider handling this status differently when
    // implementing opt-in logic.
    case kDisabledNeedsOptIn:
      return false;
    case kEligible:
      return true;
  }
  NOTREACHED();
}

[[nodiscard]] bool IsPersonalContextToggleOn(const PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }
  return pref_service->GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus);
}

// Returns the set of eligible subscription tiers configured by the
// `kAutofillAtMemoryEligibleTiers` feature parameter. Returns an empty set if
// the parameter is empty, not defined, or contains no valid integers.
base::flat_set<int32_t> GetAutofillAtMemoryEligibleTiers() {
  const std::string tier_list = features::kAutofillAtMemoryEligibleTiers.Get();
  const std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
      tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<int32_t> eligible_tiers;
  eligible_tiers.reserve(tier_pieces.size());
  for (std::string_view piece : tier_pieces) {
    int32_t tier_id = 0;
    if (base::StringToInt(piece, &tier_id)) {
      eligible_tiers.push_back(tier_id);
    }
  }
  return base::flat_set<int32_t>(std::move(eligible_tiers));
}

// Returns whether the subscription tier eligibility criteria are met.
//
// Eligibility is determined by checking whether the user's tier is configured
// as eligible by the `kAutofillAtMemoryEligibleTiers` feature parameter.
//
// If the feature parameter is empty (not set or set to an empty list), this is
// interpreted as having no restrictions, in which case any subscription tier is
// eligible (and `subscription_eligibility_service` being null is also allowed).
[[nodiscard]] bool IsSubscriptionTierEligible(
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service) {
  const base::flat_set<int32_t> eligible_tiers =
      GetAutofillAtMemoryEligibleTiers();
  if (eligible_tiers.empty()) {
    return true;
  }
  if (!subscription_eligibility_service) {
    return false;
  }
  const int32_t tier =
      subscription_eligibility_service->GetAiSubscriptionTier();
  return eligible_tiers.contains(tier);
}

// Returns true if AtMemory is supported for the user.
//
// Checks that AtMemory feature flags are enabled, At-Memory eligibility
// criteria and PersonalContext eligibility criteria are met.
// Contrary to `MayPerformAtMemoryAction`, does not check user-controlled
// toggles.
[[nodiscard]] bool IsAtMemorySupported(
    personal_context::PersonalContextEnablementService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service) {
  if constexpr (!BUILDFLAG(GOOGLE_CHROME_BRANDING)) {
    return false;
  }

  if (!IsPersonalContextEligible(personal_context_service)) {
    return false;
  }

  // TODO(crbug.com/521270638) Check enterprise policy implementation.

  if (!IsSubscriptionTierEligible(subscription_eligibility_service)) {
    return false;
  }

  return true;
}

[[nodiscard]] bool SatisfiesPersonalContextToggleRequirement(
    AtMemoryAction action,
    const PrefService* pref_service) {
  switch (action) {
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
      return IsPersonalContextToggleOn(pref_service);
    case AtMemoryAction::kShowAtMemoryInSettings:
      return true;
  }
  NOTREACHED();
}

[[nodiscard]] bool ActionRequiresUrl(AtMemoryAction action) {
  switch (action) {
    case AtMemoryAction::kShowAtMemoryInSettings:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
      return false;
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
      return true;
  }
  NOTREACHED();
}

[[nodiscard]] bool IsUrlEligible(AtMemoryAction action,
                                 AutofillOptimizationGuideDecider* decider,
                                 base::optional_ref<const GURL> url) {
  if (!decider) {
    return true;
  }
  if (!ActionRequiresUrl(action)) {
    return true;
  }
  return url && !decider->ShouldBlockAtMemory(*url);
}

}  // namespace

bool MayPerformAtMemoryAction(AtMemoryAction action,
                              const AutofillClient& client,
                              base::optional_ref<const GURL> url) {
  return MayPerformAtMemoryAction(
      action, client.GetPersonalContextEnablementService(),
      client.GetSubscriptionEligibilityService(), client.GetPrefs(),
      client.GetGoogleGroupsManager(),
      client.GetAutofillOptimizationGuideDecider(), url);
}

bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    personal_context::PersonalContextEnablementService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    const PrefService* pref_service,
    const GoogleGroupsManager* google_groups_manager,
    AutofillOptimizationGuideDecider* decider,
    base::optional_ref<const GURL> url) {
  if (base::FeatureList::IsEnabled(
          features::debug::kAtMemorySkipEnablementChecks)) {
    return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
  }
  if (!IsAtMemorySupported(personal_context_service,
                           subscription_eligibility_service)) {
    return false;
  }

  if (!IsUrlEligible(action, decider, url)) {
    return false;
  }

  if (!SatisfiesPersonalContextToggleRequirement(action, pref_service)) {
    return false;
  }

  // The feature flag check must be the last check to avoid polluting
  // experiment groups. If a user is ineligible or has the personal context
  // toggle off, we should return false before querying the feature flag.
  return IsAtMemoryFeatureEnabled(google_groups_manager);
}

bool IsAtMemoryFeatureEnabled(
    const GoogleGroupsManager* google_groups_manager) {
#if !BUILDFLAG(IS_FUCHSIA)
  return google_groups_manager
             ? google_groups_manager->IsFeatureEnabledForProfile(
                   features::kAutofillAtMemory)
             : base::FeatureList::IsEnabled(features::kAutofillAtMemory);
#else
  return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
#endif
}

}  // namespace autofill
