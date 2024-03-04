// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_

namespace autofill {

class FormStructure;

// Uses context about previous and next fields to select the appropriate type
// for fields with ambiguous upload types.
// Note that the case where a single-line street address is ambiguous to address
// line 1 is handled on the server.
void DisambiguatePossibleFieldTypes(FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
