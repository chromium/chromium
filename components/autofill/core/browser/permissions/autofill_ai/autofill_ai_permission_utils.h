// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_

#include <optional>
#include <string>

#include "build/buildflag.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

class GURL;
class PrefService;

namespace signin {
class IdentityManager;
}

namespace personal_context {
enum class PersonalContextEnablementState;
}

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}

namespace syncer {
class SyncService;
}

#if !BUILDFLAG(IS_FUCHSIA)
class GoogleGroupsManager;
#endif

namespace autofill {

class AutofillClient;
class EntityDataManager;

// An AutofillAI-related action that a user may take directly or indirectly
// (e.g., IPH).
enum class AutofillAiAction {
  // Add new locally stored AutofillAI data in settings.
  kAddLocalEntityInstanceInSettings,
  // Emit AutofillAI-related crowdsourcing votes.
  kCrowdsourcingVote,
  // Edit and delete existing AutofillAI data in settings.
  kEditAndDeleteEntityInstanceInSettings,
  // Fill AutofillAI data.
  kFilling,
  // Import (i.e. saving or updating) AutofillAI data on form submission.
  kImport,
  // Show the IPH for opting into AutofillAI.
  // TODO(crbug.com/440488776): Remove. Default availability is enabled by
  // default and thus no IPH for opt-in is shown anymore.
  kIphForOptIn,
  // List existing AutofillAI data in settings.
  kListEntityInstancesInSettings,
  // Log quality metrics to the `ModelQualityLogsService`. Doesn't control
  // whether online model inference results are logged to Mqls. This is instead
  // controlled by `kServerClassificationModel`.
  kLogToMqls,
  // If AutofillAiAvailableByDefault is disabled: Opt into (and out of) the
  // AutofillAI feature.
  // If AutofillAiAvailableByDefault is enabled: Opt into online model runs and
  // MQLS logging.
  // TODO(crbug.com/440488776): Rename to kImproveAutofillAi once
  // AutofillAiAvailableByDefault is launched.
  kOptIn,
  // Used only if AutofillAiAvailableByDefault is enabled, it controls whether
  // users can opt into Autofill AI features, such as identity docs and travel
  // information. It returns false on high-level checks, such as address-pref
  // being off.
  kEnableOrDisable,
  // Trigger a run of the server classification model.
  kServerClassificationModel,
  // Access locally cached results from the server classification model.
  kUseCachedServerClassificationModelResults,
  // Whether the user can store entities in the Google Wallet server.
  kImportToWallet,
  // Whether the user should see a promotion to allow Wallet to share data with
  // Chrome.
  kWalletDataSharingPromotion,
  // Whether ambient autofill is enabled.
  kAmbientAutofill,
  // Returns true if the entity type supports personal context data.
  kTypeSupportsAmbientAutofillData,
  // Whether ambient autofill should be shown in settings.
  kShowAmbientAutofillInSettings,
  kMaxValue = kShowAmbientAutofillInSettings,
};

// Opt-in status for the AutofillAI feature.
// TODO(crbug.com/440488776): Remove the following comment once default
// availability is launched.
// Note that the feature AutofillAiAvailableByDefault is currently in the
// process of being launched. Once this is done, this enum will not represent
// whether Autofill AI is available, rather whether online model calls
// (Enhanced Autofill) are.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AutofillAiOptInStatus)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill.autofill_ai
enum class AutofillAiOptInStatus {
  kOptedOut = 0,
  kOptedIn = 1,
  kMaxValue = kOptedIn
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiOptInStatus)

// Returns whether all permission-related requirements are met for `action` and
// a given `entity_type`. This includes:
// - Feature state (`kAutofillAiWithDataSchema`, `kAutofillAiServerModel`).
// - Pref state (prefs for address Autofill, AutofillAI and the related policy
//   prefs.)
// - Account state (sign-in status).
// - Whether the `action` can be performed for the `entity_type`.
//   `entity_type` is only considered to kFilling, kIphForOptIn, kImport,
//   kImportToWallet, kTypeSupportsAmbientAutofillData and must be non-empty in
//   these cases.
// - Miscellaneous state (OTR, locale, GeoIP).
//
// See go/forms-ai:permissions for more detail.
bool MayPerformAutofillAiAction(
    const AutofillClient& client,
    AutofillAiAction action,
    std::optional<EntityType> entity_type = std::nullopt,
    std::string* debug_message = nullptr);

bool MayPerformAutofillAiAction(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    const PrefService* prefs,
    const EntityDataManager* edm,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    bool is_wallet_public_pass_storage_enabled,
    bool is_off_the_record,
    const GeoIpCountryCode& country_code,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service,
    personal_context::PersonalContextEnablementState
        personal_context_enablement_state,
    AutofillAiAction action,
    std::optional<EntityType> entity_type = std::nullopt,
    std::string* debug_message = nullptr);

// Returns the AutofillAI opt-in status for the profile and account tied to
// `client`. Opt-in status is a profile pref, but keyed by (hashed) GAIA id. In
// particular, it is always `false` for users without a signed-in primary
// account.
// TODO(crbug.com/440488776): Remove the following comment once default
// availability is launched.
// Note that the feature AutofillAiAvailableByDefault is currently in the
// process of being launched. Once this is done, this method will not control
// whether Autofill AI is available, rather whether online model calls
// (Enhanced Autofill) are.
[[nodiscard]] bool GetAutofillAiOptInStatus(const AutofillClient& client);
[[nodiscard]] bool GetAutofillAiOptInStatus(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager);

// Sets the AutofillAI opt-in status for the profile and account tied to
// `client`. Returns `false` if the opt-in status may not be changed and `true`
// otherwise.
// TODO(crbug.com/440488776): Remove the following comment once default
// availability is launched.
// Note that the feature AutofillAiAvailableByDefault is currently in the
// process of being launched. Once this is done, this method will not control
// whether Autofill AI is available, rather whether online model calls
// (Enhanced Autofill) are.
bool SetAutofillAiOptInStatus(AutofillClient& client,
                              AutofillAiOptInStatus opt_in_status);
bool SetAutofillAiOptInStatus(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    PrefService* prefs,
    const EntityDataManager* edm,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    bool is_wallet_public_pass_storage_enabled,
    bool is_off_the_record,
    const GeoIpCountryCode& country_code,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service,
    personal_context::PersonalContextEnablementState
        personal_context_enablement_state,
    AutofillAiOptInStatus opt_in_status);

// Returns true if `entity_type` is blocked by enterprise policy on `url`.
bool IsAutofillAiEntityTypeBlockedByPolicy(const AutofillClient& client,
                                           const GURL& url,
                                           EntityType entity_type);

// Checks whether Autofill AI is disabled by enterprise policy.
[[nodiscard]] bool IsAutofillAiDisabledByEnterprisePolicy(
    const PrefService* prefs);

// Checks whether Autofill AI is enabled by enterprise policy including logging.
[[nodiscard]] bool IsAutofillAiAllowedByEnterprisePolicy(
    const PrefService* prefs);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
