// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

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

AutofillClient::AutofillPolicyDataCategory GetPolicyCategory(
    AtMemoryAction action) {
  switch (action) {
    case AtMemoryAction::kRetrievePaymentsForFilling:
      return AutofillClient::AutofillPolicyDataCategory::kPayments;
    case AtMemoryAction::kRetrieveContactInfoForFilling:
      return AutofillClient::AutofillPolicyDataCategory::kContactInfo;
    case AtMemoryAction::kRetrieveIdentityDocsForFilling:
      return AutofillClient::AutofillPolicyDataCategory::kIdentityDocs;
    case AtMemoryAction::kRetrieveTravelDataForFilling:
      return AutofillClient::AutofillPolicyDataCategory::kTravel;
    case AtMemoryAction::kRetrieveShoppingDataForFilling:
      return AutofillClient::AutofillPolicyDataCategory::kShopping;
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kShowAtMemoryInSettings:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
      break;
  }
  NOTREACHED();
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

[[nodiscard]] bool IsAndroidDeviceEligibleForAtMemory() {
#if BUILDFLAG(IS_ANDROID)
  const std::string model_name = base::SysInfo::HardwareModelName();
  const base::flat_set<std::string> enabled_devices =
      base::SplitString(features::kAutofillAtMemoryEnabledDevices.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return enabled_devices.contains(model_name);
#else
  return false;
#endif
}

// Returns whether the subscription tier eligibility or device eligibility
// criteria are met.
//
// Eligibility is determined by checking whether the user's tier is configured
// as eligible by the `kAutofillAtMemoryEligibleTiers` feature parameter, or if
// the device is a premium device configured as eligible by the
// `kAutofillAtMemoryEnabledDevices` feature parameter.
//
// If the eligible tiers feature parameter is empty (not set or set to an empty
// list), this is interpreted as having no restrictions, in which case any
// subscription tier or any device is eligible.
[[nodiscard]] bool IsSubscriptionOrDeviceEligible(
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
  if (!eligible_tiers.contains(tier) && !IsAndroidDeviceEligibleForAtMemory()) {
    MaybeOutputReason(debug_message,
                      "User subscription tier is not eligible and device is "
                      "not eligible.");
    return false;
  }
  return true;
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

  if (!IsPersonalContextEligible(personal_context_service, debug_message)) {
    return false;
  }

  if (!IsSubscriptionOrDeviceEligible(subscription_eligibility_service,
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
    case AtMemoryAction::kRetrievePaymentsForFilling:
    case AtMemoryAction::kRetrieveContactInfoForFilling:
    case AtMemoryAction::kRetrieveIdentityDocsForFilling:
    case AtMemoryAction::kRetrieveTravelDataForFilling:
    case AtMemoryAction::kRetrieveShoppingDataForFilling:
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
    case AtMemoryAction::kRetrievePaymentsForFilling:
    case AtMemoryAction::kRetrieveContactInfoForFilling:
    case AtMemoryAction::kRetrieveIdentityDocsForFilling:
    case AtMemoryAction::kRetrieveTravelDataForFilling:
    case AtMemoryAction::kRetrieveShoppingDataForFilling:
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

std::optional<AtMemoryAction> MapCategoryToAtMemoryAction(
    AutofillClient::AutofillPolicyDataCategory category) {
  switch (category) {
    case AutofillClient::AutofillPolicyDataCategory::kPayments:
      return AtMemoryAction::kRetrievePaymentsForFilling;
    case AutofillClient::AutofillPolicyDataCategory::kContactInfo:
      return AtMemoryAction::kRetrieveContactInfoForFilling;
    case AutofillClient::AutofillPolicyDataCategory::kIdentityDocs:
      return AtMemoryAction::kRetrieveIdentityDocsForFilling;
    case AutofillClient::AutofillPolicyDataCategory::kTravel:
      return AtMemoryAction::kRetrieveTravelDataForFilling;
    case AutofillClient::AutofillPolicyDataCategory::kShopping:
      return AtMemoryAction::kRetrieveShoppingDataForFilling;
  }
  NOTREACHED();
}

[[nodiscard]] bool SatisfiesOffTheRecordRequirement(
    AtMemoryAction action,
    bool is_off_the_record,
    std::string* debug_message) {
  switch (action) {
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
    case AtMemoryAction::kRetrievePaymentsForFilling:
    case AtMemoryAction::kRetrieveContactInfoForFilling:
    case AtMemoryAction::kRetrieveIdentityDocsForFilling:
    case AtMemoryAction::kRetrieveTravelDataForFilling:
    case AtMemoryAction::kRetrieveShoppingDataForFilling:
      if (is_off_the_record) {
        MaybeOutputReason(debug_message, "Off the record.");
        return false;
      }
      break;
    case AtMemoryAction::kShowAtMemoryInSettings:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
      break;
  }
  return true;
}

}  // namespace

[[nodiscard]] bool IsRetrieveForFillingAction(AtMemoryAction action) {
  switch (action) {
    case AtMemoryAction::kRetrievePaymentsForFilling:
    case AtMemoryAction::kRetrieveContactInfoForFilling:
    case AtMemoryAction::kRetrieveIdentityDocsForFilling:
    case AtMemoryAction::kRetrieveTravelDataForFilling:
    case AtMemoryAction::kRetrieveShoppingDataForFilling:
      return true;
    case AtMemoryAction::kShowAtMemoryInSettings:
    case AtMemoryAction::kAllowCustomizeAtMemoryShortcut:
    case AtMemoryAction::kTriggerSearchUI:
    case AtMemoryAction::kShowIph:
    case AtMemoryAction::kShowAutocompleteAtMemoryButton:
      return false;
  }
  NOTREACHED();
}

std::optional<AtMemoryAction> ToAtMemoryRetrieveForFillingAction(
    MemoryDataType type) {
  return ToAutofillPolicyDataCategory(type).and_then(
      &MapCategoryToAtMemoryAction);
}

bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    const AutofillClient& client,
    base::optional_ref<const GURL> url,
    base::optional_ref<const RetrieveForFillingParams> retrieve_params,
    std::string* debug_message) {
  if (IsRetrieveForFillingAction(action)) {
    if (!retrieve_params.has_value()) {
      DCHECK(false) << "retrieve_params must be provided for retrieve actions.";
      return false;
    }
  } else {
    DCHECK(!retrieve_params.has_value())
        << "retrieve_params must not be provided for non-retrieve actions.";
  }

  if (retrieve_params.has_value()) {
    if (retrieve_params->is_spii) {
      bool reauth_ok = client.SupportsDeviceReauth() ||
                       base::FeatureList::IsEnabled(
                           features::debug::kAtMemoryNoDeviceReauthCheck);
      if (!retrieve_params->is_context_secure || !reauth_ok) {
        MaybeOutputReason(
            debug_message,
            "SPII data is not allowed in insecure contexts or when "
            "device reauth is not supported.");
        return false;
      }
    }

    bool comes_from_autofill =
        std::ranges::any_of(retrieve_params->sources, [](const auto& source) {
          return source.type == MemoryEntrySourceType::kAutofill;
        });
    if (comes_from_autofill) {
      AutofillClient::AutofillPolicyDataCategory category =
          GetPolicyCategory(action);
      const GURL& target_url =
          url ? *url : client.GetLastCommittedPrimaryMainFrameURL();
      if (client.IsAutofillTypeBlockedByPolicy(target_url, category)) {
        return false;
      }
    }
  }

  return MayPerformAtMemoryActionBase(
      action, client.GetPersonalContextEligibilityService(),
      client.GetSubscriptionEligibilityService(), client.GetPrefs(),
      client.GetGoogleGroupsManager(),
      client.GetAutofillOptimizationGuideDecider(), client.IsOffTheRecord(),
      url, debug_message);
}

bool MayPerformAtMemoryActionBase(
    AtMemoryAction action,
    personal_context::PersonalContextEligibilityService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    const PrefService* pref_service,
    const GoogleGroupsManager* google_groups_manager,
    AutofillOptimizationGuideDecider* decider,
    bool is_off_the_record,
    base::optional_ref<const GURL> url,
    std::string* debug_message) {
  if (base::FeatureList::IsEnabled(
          features::debug::kAtMemorySkipEnablementChecks)) {
    return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
  }

  if (!SatisfiesOffTheRecordRequirement(action, is_off_the_record,
                                        debug_message)) {
    return false;
  }

  if (!IsAtMemorySupported(personal_context_service,
                           subscription_eligibility_service, debug_message)) {
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
