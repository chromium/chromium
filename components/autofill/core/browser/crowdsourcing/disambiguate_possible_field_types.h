// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_

namespace autofill {

class FormStructure;

// Applies several heuristics to select the most probable types for fields with
// ambiguous possible types. Heuristics are run in order of priority which is
// based on reflecting user intent the most.
// Note that the case where a single-line street address is ambiguous to address
// line 1 is handled on the server.
void DisambiguatePossibleFieldTypes(FormStructure& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
