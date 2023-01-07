// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_RATIONALIZATION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_RATIONALIZATION_UTIL_H_

#include <vector>

namespace autofill {

class AutofillField;

namespace rationalization_util {

// Helper function that rationalizes phone numbers fields in the vector of
// fields. It ensures that only a single phone number is autofilled in a
// section as part of a form fill operation. If the section contains multiple
// phone numbers, set_only_fill_when_focused(true) is called on the remaining
// fields.
//
// The vector of fields are expected to contain all fields of a certain section.
void RationalizePhoneNumberFields(
    const std::vector<AutofillField*>& fields_in_section);

}  // namespace rationalization_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_RATIONALIZATION_UTIL_H_
