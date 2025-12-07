// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_

#include <vector>

#include "base/containers/span.h"

namespace autofill {

class AutofillField;
struct PossibleTypes;

// Applies several heuristics to select the most probable types for fields with
// ambiguous possible types. Heuristics are run in order of priority which is
// based on reflecting user intent the most.
// Note that the case where a single-line street address is ambiguous to address
// line 1 is handled on the server.
std::vector<PossibleTypes> DisambiguatePossibleFieldTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    std::vector<PossibleTypes> possible_types);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DISAMBIGUATE_POSSIBLE_FIELD_TYPES_H_
