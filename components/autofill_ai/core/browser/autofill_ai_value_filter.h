// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_VALUE_FILTER_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_VALUE_FILTER_H_

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace autofill_ai {

// Applies filtering rules to remove potential SPII data from form fields.
// Return the number of fields that have been filtered.
void FilterSensitiveValues(autofill::FormStructure& form);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_VALUE_FILTER_H_
