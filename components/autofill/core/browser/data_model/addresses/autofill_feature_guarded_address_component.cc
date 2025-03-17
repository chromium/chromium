// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_feature_guarded_address_component.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

FeatureGuardedAddressComponent::FeatureGuardedAddressComponent(
    raw_ptr<const base::Feature> feature,
    FieldType storage_type,
    SubcomponentsList children,
    unsigned int merge_mode)
    : AddressComponent(storage_type, std::move(children), merge_mode),
      feature_(feature) {}

void FeatureGuardedAddressComponent::SetValue(std::u16string value,
                                              VerificationStatus status) {
  if (!base::FeatureList::IsEnabled(*feature_)) {
    return;
  }
  AddressComponent::SetValue(std::move(value), status);
}

FieldTypeSet FeatureGuardedAddressComponent::GetTypes(
    bool storable_only) const {
  if (!base::FeatureList::IsEnabled(*feature_)) {
    return {};
  }
  return AddressComponent::GetTypes(storable_only);
}

}  // namespace autofill
