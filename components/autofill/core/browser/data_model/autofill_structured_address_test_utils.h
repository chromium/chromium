// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace structured_address {

// Defines the type, value and verification status of a structured address
// component for testing.
struct AddressComponentTestValue {
  ServerFieldType type;
  std::string value;
  VerificationStatus status;
};

// Sets the supplied test values.
void SetTestValues(AddressComponent* component,
                   const std::vector<AddressComponentTestValue>& test_values,
                   bool finalize = true);

// Verifies that all values and verification statuses in |test_values| match
// the values of the structured |component|.
void VerifyTestValues(AddressComponent* component,
                      const std::vector<AddressComponentTestValue> test_values);

}  // namespace structured_address
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_TEST_UTILS_H_
