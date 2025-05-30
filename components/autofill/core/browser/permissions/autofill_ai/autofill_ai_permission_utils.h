// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_

namespace autofill {

class AutofillClient;

// An AutofillAI-related action that a user may take directly or indirectly
// (e.g., IPH).
enum class AutofillAiAction {
  // Add new AutofillAI data in settings.
  kAddEntityInstanceInSettings,
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
  kUseCachedServerClassificationModelResults
};

// Returns whether all permission-related requirements are met for `action`.
// This includes:
// - Feature state (`kAutofillAiWithDataSchema`, `kAutofillAiServerModel`).
// - Pref state (prefs for address Autofill, AutofillAI and the related policy
//   prefs.)
// - Account state (sign-in status, model execution capabilities).
// - Miscellaneous state (OTR, locale, GeoIP).
//
// See go/forms-ai:permissions for more detail.
bool MayPerformAutofillAiAction(const AutofillClient& client,
                                AutofillAiAction action);

// Returns the AutofillAI opt-in status for the profile and account tied to
// `client`. Opt-in status is a profile pref, but keyed by (hashed) GAIA id. In
// particular, it is always `false` for users without a signed-in primary
// account.
[[nodiscard]] bool GetAutofillAiOptInStatus(const AutofillClient& client);

// Sets the AutofillAI opt-in status for the profile and account tied to
// `client`. Returns `false` if the opt-in status may not be changed and `true`
// otherwise.
bool SetAutofillAiOptInStatus(AutofillClient& client, bool opt_in_status);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
