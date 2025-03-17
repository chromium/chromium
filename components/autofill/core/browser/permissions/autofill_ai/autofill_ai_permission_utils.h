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
  kAddEntityInstanceInSettings,
  kCrowdsourcingVote,
  kEditAndDeleteEntityInstanceInSettings,
  kFilling,
  kImport,
  kIphForOptIn,
  kListEntityInstancesInSettings,
  kOptIn,
  kServerClassificationModel,
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

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERMISSION_UTILS_H_
