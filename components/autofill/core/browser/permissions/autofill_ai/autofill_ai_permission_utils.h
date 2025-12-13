// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_

#include <optional>
#include <string>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace autofill {

class AutofillClient;

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
  kIphForOptIn,
  // List existing AutofillAI data in settings.
  kListEntityInstancesInSettings,
  // Log data to the `ModelQualityLogsService`.
  kLogToMqls,
  // Opt into (and out of) the AutofillAI feature.
  kOptIn,
  // Trigger a run of the server classification model.
  kServerClassificationModel,
  // Access locally cached results from the server classification model.
  kUseCachedServerClassificationModelResults,
  // Whether the user can store entities in the Google Wallet server.
  kImportToWallet,
};

// Opt-in status for the AutofillAI feature.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AutofillAiOptInStatus)
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
// - Account state (sign-in status, model execution capabilities).
// - Whether the `action` can be performed for the `entity_type`.
//   `entity_type` is only considered to kFilling, kIphForOptIn, kImport,
//   kImportToWallet and must be non-empty in these cases.
// - Miscellaneous state (OTR, locale, GeoIP).
//
// See go/forms-ai:permissions for more detail.
bool MayPerformAutofillAiAction(
    const AutofillClient& client,
    AutofillAiAction action,
    std::optional<EntityType> entity_type = std::nullopt,
    std::string* debug_message = nullptr);

// Returns the AutofillAI opt-in status for the profile and account tied to
// `client`. Opt-in status is a profile pref, but keyed by (hashed) GAIA id. In
// particular, it is always `false` for users without a signed-in primary
// account.
[[nodiscard]] bool GetAutofillAiOptInStatus(const AutofillClient& client);
[[nodiscard]] bool GetAutofillAiOptInStatus(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager);
// Similar to `GetAutofillAiOptInStatus()` but always uses the pref that is
// currently being deprecated (`prefs::kAutofillAiOptInStatus`). This method
// should only be used at start-up time to migrate the old pref value to the new
// one.
[[nodiscard]] bool GetAutofillAiOptInStatusFromNonSyncingPref(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager);

// Sets the AutofillAI opt-in status for the profile and account tied to
// `client`. Returns `false` if the opt-in status may not be changed and `true`
// otherwise.
bool SetAutofillAiOptInStatus(AutofillClient& client,
                              AutofillAiOptInStatus opt_in_status);

// Returns whether the user has ever explicitly opted in or out of Autofill AI.
//
// This is only intended to be used during migration from local to synced prefs.
[[nodiscard]] bool HasSetLocalAutofillAiOptInStatus(
    const PrefService* prefs,
    const signin::IdentityManager* identity_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
