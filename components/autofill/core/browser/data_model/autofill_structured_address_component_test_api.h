// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_TEST_API_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

// Exposes some testing operations for AddressComponent.
class AddressComponentTestApi {
 public:
  explicit AddressComponentTestApi(AddressComponent& component)
      : component_(component) {}

  // Initiates the formatting of the values from the subcomponents.
  void FormatValueFromSubcomponents() {
    component_->FormatValueFromSubcomponents();
  }

  // Returns the best format string for testing.
  std::u16string GetFormatString() const {
    return component_->GetFormatString();
  }

  // Returns the parse expressions by relevance for testing.
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance() {
    return component_->GetParseRegularExpressionsByRelevance();
  }

  // Returns a reference to the root node of the tree for testing.
  AddressComponent& GetRootNode() { return component_->GetRootNode(); }

  // Returns a vector containing the |storage_types_| of all direct
  // subcomponents.
  std::vector<FieldType> GetSubcomponentTypes() const {
    return component_->GetSubcomponentTypes();
  }

  // Sets the merge mode for testing purposes.
  void SetMergeMode(int merge_mode) { component_->merge_mode_ = merge_mode; }

  // Returns the value used for comparison for testing purposes.
  std::u16string GetValueForComparison(const AddressComponent& other) const {
    return component_->GetValueForComparison(other);
  }

  AddressComponent* GetNodeForType(FieldType field_type) {
    return component_->GetNodeForType(field_type);
  }

 private:
  raw_ref<AddressComponent> component_;
};

inline AddressComponentTestApi test_api(AddressComponent& component) {
  return AddressComponentTestApi(component);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_TEST_API_H_
