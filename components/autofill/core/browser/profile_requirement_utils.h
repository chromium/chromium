// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

// Stores the collection of AddressProfileImportRequirementMetric that are
// violated. These violation prevents the import of a profile.
constexpr autofill_metrics::AddressProfileImportRequirementMetric
    kMinimumAddressRequirementViolations[] = {
        autofill_metrics::AddressProfileImportRequirementMetric::
            kLine1RequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kCityRequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kStateRequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kZipRequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kZipOrStateRequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kLine1OrHouseNumberRequirementViolated,
        autofill_metrics::AddressProfileImportRequirementMetric::
            kNameRequirementViolated};

// Does requirement checks on the `profile`. Either the requirements are
// fulfilled or they are violated. An address submitted via a form must have at
// least the fields required as determined by its country code. No verification
// of validity of the contents is performed. This is an existence check only.
// Assumes `profile` has been finalized.
// Introducing additional profile import checks should be complemented with
// adding to the violations list in
// `autofill_metrics::kMinimumAddressRequirementViolations`.
base::flat_set<autofill_metrics::AddressProfileImportRequirementMetric>
GetAutofillProfileRequirementResult(const AutofillProfile& profile,
                                    LogBuffer* import_log_buffer);

// Returns true if minimum requirements for import of a given `profile` have
// been met.
// Uses the country_code from `profile` to fetch the requirements and the run
// the validations.
bool IsMinimumAddress(const AutofillProfile& profile);

// Returns true if the profile can be migrated to the Account. Only sufficiently
// complete profiles are migrated and this method does not check for the
// completeness of the `profile`.
bool IsEligibleForMigrationToAccount(
    const PersonalDataManager& personal_data_manager,
    const AutofillProfile& profile);
}  //  namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_REQUIREMENT_UTILS_H_
