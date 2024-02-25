// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Defines the type, value and verification status of a structured address
// component for testing.
struct AddressComponentTestValue {
  FieldType type;
  std::string value;
  VerificationStatus status;
};

// Test the merging of two AddressComponents. |older_component| is considered to
// be older and |newer_component| is the newer one. In terms of the resulting
// AddressComponent, the expectation is defined by |merge_expectation| while
// |is_mergeable| defines the expectation if the two components can be merged at
// all. With |merge_mode|, the applied merge strategies are defined and
// |newer_was_more_recently_used| defines if the newer component is also the
// most recently used one.
void TestMerging(
    AddressComponent* older_component,
    AddressComponent* newer_component,
    const std::vector<AddressComponentTestValue>& merge_expectation,
    bool is_mergeable = true,
    int merge_modes = MergeMode::kRecursivelyMergeTokenEquivalentValues,
    bool newer_was_more_recently_used = true);

// Sets the supplied test values.
void SetTestValues(AddressComponent* component,
                   const std::vector<AddressComponentTestValue>& test_values,
                   bool finalize = true);

// Verifies that all values and verification statuses in |test_values| match
// the values of the structured |component|.
void VerifyTestValues(AddressComponent* component,
                      const std::vector<AddressComponentTestValue> test_values);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_
