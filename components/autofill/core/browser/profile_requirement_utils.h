// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_

#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

class AddressDataManager;

// Checks the country-specific import requirements of the `profile` by ensuring
// that certain types have a non-empty value. For each requirement the function
// returns either the fulfilled or violated enum entry. An address submitted via
// a form must have at least the fields required as determined by its country
// code. No verification of validity of the contents is performed. This is an
// existence check only. Assumes `profile` has been finalized. Introducing
// additional profile import checks should be complemented with adding to the
// violations list in `kMinimumAddressRequirementViolations`. If `log_buffer` is
// present, validation results are logged there.
std::vector<autofill_metrics::AddressProfileImportRequirementMetric>
ValidateProfileImportRequirements(const AutofillProfile& profile,
                                  LogBuffer* log_buffer = nullptr);

// Validates non-empty values for certain types (e.g. is the email address
// an actual email address). Emits metrics for all violate (= non-empty and
// invalid) types.
// Returns true if all non-empty values are valid.
bool ValidateNonEmptyValues(const AutofillProfile& profile,
                            LogBuffer* log_buffer);

// Returns true if the minimum requirements to import the `profile` are met.
// If `log_buffer` is present, validation results are logged there.
bool IsMinimumAddress(const AutofillProfile& profile,
                      LogBuffer* log_buffer = nullptr);

// Returns true if the profile can be migrated to the Account. Only sufficiently
// complete profiles are migrated and this method does not check for the
// completeness of the `profile`.
bool IsEligibleForMigrationToAccount(
    const AddressDataManager& address_data_manager,
    const AutofillProfile& profile);

// Returns true if the profile is eligible to be migrated to the Account. Does
// not check if the user is eligible for account storage.
bool IsProfileEligibleForMigrationToAccount(
    const AddressDataManager& address_data_manager,
    const AutofillProfile& profile);
}  //  namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_
