// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_

namespace autofill {
class FormStructure;
class AutofillField;
}  // namespace autofill

namespace autofill_ai {

// Returns true if the `field` is eligible based on the type criteria.
bool IsFieldEligibleByTypeCriteria(const autofill::AutofillField& field);

// For a field to be fillable
//  - it must have the correct field type.
//  - its value must be empty.
//  - it has to be focusable. In field filling skip reasons select fields may
//    be unfocusable, for the estimated `total_number_of_fillable_fields`
//    however that exception has shown to offer suggestions too often.
bool IsFieldEligibleForFilling(const autofill::AutofillField& form_field);

// Returns weather the forms is eligible for the filling journey.
bool IsFormEligibleForFilling(const autofill::FormStructure& form);

// Set the filling eligibility of individual fields.
void SetFieldFillingEligibility(autofill::FormStructure& form);

// Return weather the forms is eligible for the import journey.
bool IsFormEligibleForImportByFieldCriteria(
    const autofill::FormStructure& form);
}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
