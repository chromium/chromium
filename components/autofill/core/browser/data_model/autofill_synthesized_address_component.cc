// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_synthesized_address_component.h"

namespace autofill {

SynthesizedAddressComponent::SynthesizedAddressComponent(
    FieldType storage_type,
    SubcomponentsList children,
    unsigned int merge_mode)
    : AddressComponent(storage_type, {}, merge_mode) {
  for (AddressComponent* child : children) {
    RegisterChildNode(child, /*set_as_parent_of_child=*/false);
  }
}

void SynthesizedAddressComponent::GetTypes(
    bool storable_only,
    FieldTypeSet* supported_types) const {
  if (storable_only) {
    return;
  }
  supported_types->insert(GetStorageType());
}

bool SynthesizedAddressComponent::IsValueReadOnly() const {
  return true;
}

}  // namespace autofill
