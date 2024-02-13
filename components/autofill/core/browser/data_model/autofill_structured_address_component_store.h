// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_STORE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_STORE_H_

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Registry of AddressComponent nodes keyed by their corresponding `FieldType`.
class AddressComponentsStore {
 public:
  AddressComponentsStore();
  explicit AddressComponentsStore(
      base::flat_map<FieldType, std::unique_ptr<AddressComponent>> components);

  AddressComponentsStore(const AddressComponentsStore&) = delete;
  AddressComponentsStore(AddressComponentsStore&&);
  AddressComponentsStore& operator=(const AddressComponentsStore&) = delete;
  AddressComponentsStore& operator=(AddressComponentsStore&&);

  ~AddressComponentsStore();

  // Returns the node in the tree that supports `field_type`. This node, if it
  // exists, is unique by definition. Returns nullptr if no such node exists.
  AddressComponent* GetNodeForType(FieldType field_type) const;

  AddressComponent* Root() const {
    return components_.at(ADDRESS_HOME_ADDRESS).get();
  }

  // Wipes all `AddressComponent` internal pointers, in order to avoid dangling
  // pointers while the `components_` is deleted.
  void WipeRawPtrsForDestruction() {
    for (const auto& it : components_) {
      it.second->WipeRawPtrsForDestruction();
    }
  }

 private:
  base::flat_map<FieldType, std::unique_ptr<AddressComponent>> components_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_STORE_H_
