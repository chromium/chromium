// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"

#include <ostream>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component_test_api.h"

namespace autofill {

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

std::ostream& operator<<(std::ostream& out, const AddressComponent& component) {
  out << "type=" << component.GetStorageTypeName()
      << ", value=" << base::UTF16ToUTF8(component.GetValue())
      << ", status=" << static_cast<int>(component.GetVerificationStatus())
      << std::endl;
  for (const AddressComponent* sub_component : component.Subcomponents()) {
    out << "\t" << *sub_component;
  }
  return out;
}

void TestMerging(
    AddressComponent* older_component,
    AddressComponent* newer_component,
    const std::vector<AddressComponentTestValue>& merge_expectation,
    bool is_mergeable,
    int merge_modes,
    bool newer_was_more_recently_used) {
  test_api(*older_component).SetMergeMode(merge_modes);

  SCOPED_TRACE(is_mergeable);
  SCOPED_TRACE(merge_modes);
  SCOPED_TRACE(*older_component);
  SCOPED_TRACE(*newer_component);

  EXPECT_EQ(is_mergeable,
            older_component->IsMergeableWithComponent(*newer_component));
  EXPECT_EQ(is_mergeable, older_component->MergeWithComponent(
                              *newer_component, newer_was_more_recently_used));
  VerifyTestValues(older_component, merge_expectation);
}

void SetTestValues(AddressComponent* component,
                   const AddressComponentTestValues& test_values,
                   bool finalize) {
  for (const auto& test_value : test_values) {
    component->SetValueForType(test_value.type,
                               base::UTF8ToUTF16(test_value.value),
                               test_value.status);
  }
  if (finalize)
    component->CompleteFullTree();
}

void VerifyTestValues(AddressComponent* component,
                      const AddressComponentTestValues test_values) {
  for (const auto& test_value : test_values) {
    SCOPED_TRACE(base::StringPrintf("Failed type=%s, value=%s, status=%d",
                                    FieldTypeToString(test_value.type).c_str(),
                                    test_value.value.c_str(),
                                    static_cast<int>(test_value.status)));

    EXPECT_EQ(base::UTF16ToUTF8(component->GetValueForType(test_value.type)),
              test_value.value);

    // Omit testing the status if the value is empty.
    if (!test_value.value.empty()) {
      EXPECT_EQ(component->GetVerificationStatusForType(test_value.type),
                test_value.status);
    }
  }
}

}  // namespace autofill
