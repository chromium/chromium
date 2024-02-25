// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_component_store.h"

namespace autofill {

AddressComponentsStore::AddressComponentsStore() = default;
AddressComponentsStore::AddressComponentsStore(
    base::flat_map<FieldType, std::unique_ptr<AddressComponent>> components)
    : components_(std::move(components)) {}

// To prevent dangling pointers during the deletion of the `components_`, it's
// crucial to clear all internal AddressComponent pointers. Since the deletion
// order of nodes within the `components_` is unpredictable, this step ensures
// that no AddressComponent (which may reference other components like parents
// or children) retains invalid pointers after the components_ is destroyed.
AddressComponentsStore::AddressComponentsStore(AddressComponentsStore&& other) {
  WipeRawPtrsForDestruction();
  components_ = std::move(other.components_);
}

AddressComponent* AddressComponentsStore::GetNodeForType(
    FieldType field_type) const {
  if (auto it = components_.find(field_type); it != components_.end()) {
    return it->second.get();
  }
  return nullptr;
}

AddressComponentsStore& AddressComponentsStore::operator=(
    AddressComponentsStore&& other) {
  WipeRawPtrsForDestruction();
  components_ = std::move(other.components_);
  return *this;
}

AddressComponentsStore::~AddressComponentsStore() {
  WipeRawPtrsForDestruction();
}

}  // namespace autofill
