// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_UTILS_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_UTILS_H_

namespace autofill {
class FormStructure;
class AutofillField;
}  // namespace autofill

namespace autofill_prediction_improvements {

// Returns true if the `field` is eligible based on the type criteria.
bool IsFieldEligibleByTypeCriteria(const autofill::AutofillField& field);

// Return weather the forms is eligible for the filling journey.
bool IsFormEligibleForFillingByFieldCriteria(
    const autofill::FormStructure& form);

// Return weather the forms is eligible for the import journey.
bool IsFormEligibleForImportByFieldCriteria(
    const autofill::FormStructure& form);

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_UTILS_H_
