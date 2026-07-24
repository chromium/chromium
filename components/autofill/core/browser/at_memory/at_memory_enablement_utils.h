// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

class GoogleGroupsManager;
class GURL;
class PrefService;

namespace personal_context {
class PersonalContextEligibilityService;
}  // namespace personal_context

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}  // namespace subscription_eligibility

namespace autofill {

// An AtMemory-related action that a user may take (directly or indirectly).
// LINT.IfChange(AtMemoryAction)
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
  // Retrieve/Fill Payments data.
  kRetrievePaymentsForFilling,
  // Retrieve/Fill Contact Info data.
  kRetrieveContactInfoForFilling,
  // Retrieve/Fill Identity Docs data.
  kRetrieveIdentityDocsForFilling,
  // Retrieve/Fill Travel data.
  kRetrieveTravelDataForFilling,
  // Retrieve/Fill Shopping data.
  kRetrieveShoppingDataForFilling,
};
// LINT.ThenChange(/chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.cc:AtMemoryAction,
// /components/autofill/core/browser/autofill_and_password_manager_internals/autofill_and_password_manager_internals.ts:AtMemoryAction)

// Parameters used specifically for evaluating actions that retrieve AtMemory
// data for filling (e.g. `AtMemoryAction::kRetrievePaymentsForFilling`).
struct RetrieveForFillingParams {
  STACK_ALLOCATED();

 public:
  bool is_spii = false;
  base::span<const MemoryEntrySource> sources;
  bool is_context_secure = false;
};

class AutofillOptimizationGuideDecider;

// Returns true if the action retrieves data for filling.
[[nodiscard]] bool IsRetrieveForFillingAction(AtMemoryAction action);

// Translates an entry type from the accessibility annotator to an
// Autofill-specific AtMemoryAction for retrieving data for filling.
std::optional<AtMemoryAction> ToAtMemoryRetrieveForFillingAction(
    MemoryDataType type);

// Returns whether all permission-related requirements are met for `action`.
//
// Checks that AtMemory feature flags are enabled, AtMemory eligibility
// criteria are met and PersonalContext settings toggle is on if required by
// the action.
[[nodiscard]] bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    const AutofillClient& client,
    base::optional_ref<const GURL> url = std::nullopt,
    base::optional_ref<const RetrieveForFillingParams> retrieve_params =
        std::nullopt,
    std::string* debug_message = nullptr);

// Returns whether the base permission-related requirements are met for
// `action`.
//
// Note: For retrieve-for-filling actions, specific security/policy checks (e.g.
// SPII device reauth or Enterprise policy constraints) are ONLY evaluated by
// `MayPerformAtMemoryAction` above. This base function only verifies core
// eligibility (feature flags, preferences, etc).
// TODO(crbug.com/503254452) Figure out how these 2 functions should be named
// and work together.
[[nodiscard]] bool MayPerformAtMemoryActionBase(
    AtMemoryAction action,
    personal_context::PersonalContextEligibilityService*
        personal_context_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_eligibility_service,
    const PrefService* pref_service,
    const GoogleGroupsManager* google_groups_manager,
    AutofillOptimizationGuideDecider* decider,
    base::optional_ref<const GURL> url = std::nullopt,
    std::string* debug_message = nullptr);

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
