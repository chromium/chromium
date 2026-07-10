// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
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

// Helper function for debugging why a permissions check failed.
void MaybeOutputReason(std::string* out, std::string_view message) {
  if (out) {
    *out = std::string(message);
  }
}

[[nodiscard]] bool IsPersonalContextEligible(
    personal_context::PersonalContextEligibilityService*
        personal_context_service,
    std::string* debug_message) {
  if (!personal_context_service) {
    MaybeOutputReason(debug_message,
                      "Personal Context service is not available.");
    return false;
  }
  using enum personal_context::PersonalContextEligibilityState;
  switch (personal_context_service->GetEligibilityState()) {
    case kDisabledNotEligible:
    // TODO(crbug.com/504893949) Consider handling this status differently when
    // implementing opt-in logic.
    case kDisabledNeedsOptIn:
      MaybeOutputReason(debug_message,
                        "User is not eligible for Personal Context.");
      return false;
    case kEligible:
      return true;
  }
  NOTREACHED();
}

[[nodiscard]] bool IsPersonalContextToggleOn(const PrefService* pref_service,
                                             std::string* debug_message) {
  if (!pref_service) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return false;
  }
  if (!pref_service->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    MaybeOutputReason(debug_message,
                      "Personal Context settings toggle is off.");
    return false;
  }
  return true;
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
        subscription_eligibility_service,
    std::string* debug_message) {
  const base::flat_set<int32_t> eligible_tiers =
      GetAutofillAtMemoryEligibleTiers();
  if (eligible_tiers.empty()) {
    return true;
  }
  if (!subscription_eligibility_service) {
    MaybeOutputReason(debug_message,
                      "Subscription eligibility service not available.");
    return false;
  }
  const int32_t tier =
      subscription_eligibility_service->GetAiSubscriptionTier();
  if (!eligible_tiers.contains(tier)) {
    MaybeOutputReason(debug_message, "User subscription tier is not eligible.");
    return false;
  }
  return true;
}

// Returns whether enterprise policies allow AtMemory trigger.
//
// AtMemory is disabled for the Enterprise accounts and these are blocked in the
// `PersonalContextService`. Additional checks are performed here to ensure
// correct behavior for consumer accounts on enterprise devices.
[[nodiscard]] bool SatisfiesEnterprisePolicies(const PrefService* pref_service,
                                               std::string* debug_message) {
  if (!pref_service) {
    MaybeOutputReason(debug_message, "Prefs are not available.");
    return false;
  }

  // TODO(crbug.com/521270638) Add a check for the AtMemory specific policy on
  // top of the enterprise policy for Gemini.

  constexpr int kGeminiSettingsAvailable = 0;
  // TODO(crbug.com/393537628) Move the pref values enum to components and use
  // this value here.
  const bool gemini_settings_allowed =
      pref_service->GetInteger(optimization_guide::prefs::kGeminiSettings) ==
      kGeminiSettingsAvailable;
  if (!gemini_settings_allowed) {
    MaybeOutputReason(debug_message,
                      "Disallowed by GeminiSettings enterprise policy.");
  }
  return gemini_settings_allowed;
}

// Returns true if AtMemory is supported for the user.
//
// Checks that AtMemory feature flags are enabled, At-Memory eligibility
// criteria and PersonalContext eligibility criteria are met.
// Contrary to `MayPerformAtMemoryAction`, does not check user-controlled
// nor admin-controlled toggles.
[[nodiscard]] bool IsAtMemorySupported(
    personal_context::PersonalContextEligibilityService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    std::string* debug_message) {
  if constexpr (!BUILDFLAG(GOOGLE_CHROME_BRANDING)) {
    MaybeOutputReason(debug_message, "Not a branded Chrome build.");
    return false;
  }

  if (!IsPersonalContextEligible(personal_context_service, debug_message)) {
    return false;
  }

  if (!IsSubscriptionTierEligible(subscription_eligibility_service,
                                  debug_message)) {
    return false;
  }

  return true;
}

[[nodiscard]] bool SatisfiesPersonalContextToggleRequirement(
    AtMemoryAction action,
    const PrefService* pref_service,
    std::string* debug_message) {
  switch (action) {
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
      return IsPersonalContextToggleOn(pref_service, debug_message);
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
                                 base::optional_ref<const GURL> url,
                                 std::string* debug_message) {
  if (!decider) {
    return true;
  }
  if (!ActionRequiresUrl(action)) {
    return true;
  }
  if (!url) {
    MaybeOutputReason(debug_message, "URL is not available.");
    return false;
  }
  if (decider->ShouldBlockAtMemory(*url)) {
    MaybeOutputReason(debug_message, "URL is blocklisted.");
    return false;
  }
  return true;
}

}  // namespace

bool MayPerformAtMemoryAction(AtMemoryAction action,
                              const AutofillClient& client,
                              base::optional_ref<const GURL> url,
                              std::string* debug_message) {
  return MayPerformAtMemoryAction(
      action, client.GetPersonalContextEligibilityService(),
      client.GetSubscriptionEligibilityService(), client.GetPrefs(),
      client.GetGoogleGroupsManager(),
      client.GetAutofillOptimizationGuideDecider(), url, debug_message);
}

bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    personal_context::PersonalContextEligibilityService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    const PrefService* pref_service,
    const GoogleGroupsManager* google_groups_manager,
    AutofillOptimizationGuideDecider* decider,
    base::optional_ref<const GURL> url,
    std::string* debug_message) {
  if (base::FeatureList::IsEnabled(
          features::debug::kAtMemorySkipEnablementChecks)) {
    return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
  }
  if (!IsAtMemorySupported(personal_context_service,
                           subscription_eligibility_service, debug_message)) {
    return false;
  }

  if (!SatisfiesEnterprisePolicies(pref_service, debug_message)) {
    return false;
  }

  if (!IsUrlEligible(action, decider, url, debug_message)) {
    return false;
  }

  if (!SatisfiesPersonalContextToggleRequirement(action, pref_service,
                                                 debug_message)) {
    return false;
  }

  // The feature flag check must be the last check to avoid polluting
  // experiment groups. If a user is ineligible or has the personal context
  // toggle off, we should return false before querying the feature flag.
  if (!IsAtMemoryFeatureEnabled(google_groups_manager)) {
    MaybeOutputReason(debug_message, "AutofillAtMemory is not enabled.");
    return false;
  }
  return true;
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
