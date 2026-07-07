// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

class GoogleGroupsManager;
class GURL;
class PrefService;

namespace personal_context {
class PersonalContextEnablementService;
}  // namespace personal_context

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}  // namespace subscription_eligibility

namespace autofill {

// An AtMemory-related action that a user may take (directly or indirectly).
enum class AtMemoryAction {
  // Trigger main AtMemory component using the keyboard invocation.
  kTriggerSearchUI,
  // Show any AtMemory related settings in the Enhanced Autofill section of
  // the settings. This evaluates to true regardless of the Personal Context
  // toggle state.
  // This does not imply that the settings are functional.
  // E.g. for the AtMemoryShortcut customization there is a separate
  // `AtMemoryAction`: `kAllowCustomizeAtMemoryShortcut` to verify if it's
  // functional.
  kShowAtMemoryInSettings,
  // Allow the user to customize the AtMemory shortcut.
  // This unlocks the ability to reconfigure the shortcut in Enhanced
  // Autofill section of the Settings.
  kAllowCustomizeAtMemoryShortcut,
  // Show the In-Product Help (IPH) Promo Bubble for AtMemory.
  kShowIph,
  // Show the AtMemory button in the Autocomplete dialog.
  kShowAutocompleteAtMemoryButton,
};

class AutofillOptimizationGuideDecider;

// Returns whether all permission-related requirements are met for `action`.
//
// Checks that AtMemory feature flags are enabled, AtMemory eligibility
// criteria are met and PersonalContext settings toggle is on if required by
// the action.
[[nodiscard]] bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    const AutofillClient& client,
    base::optional_ref<const GURL> url = std::nullopt);

[[nodiscard]] bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    personal_context::PersonalContextEnablementService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    const PrefService* pref_service,
    const GoogleGroupsManager* google_groups_manager,
    AutofillOptimizationGuideDecider* decider,
    base::optional_ref<const GURL> url = std::nullopt);

// Returns whether the AtMemory feature is enabled.
//
// To be used instead of `base::FeatureList::(features::kAutofillAtMemory)` -
// use the functions above if you require more extensive permission checks.
//
// Since the AtMemory feature has a server-side component, whether the feature
// works correctly is both installation and GAIA-id specific.
// Put differently, `base::Feature` has the same state for all installed
// profiles, but the AtMemory server enforces that the user request has a
// permitted GAIA id. This function checks that both the `kAutofillAtMemory` is
// enabled and that `google_groups_manager` confirms that the user is a member
// of the relevant Google Group.
//
// If `google_groups_manager` is null, this falls back to the standard,
// profile-independent feature check.
[[nodiscard]] bool IsAtMemoryFeatureEnabled(
    const GoogleGroupsManager* google_groups_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_
