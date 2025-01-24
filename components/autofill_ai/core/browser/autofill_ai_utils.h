// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace autofill_ai {

// Returns weather the forms is eligible for the filling journey.
bool IsFormEligibleForFilling(const autofill::FormStructure& form);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_UTILS_H_
