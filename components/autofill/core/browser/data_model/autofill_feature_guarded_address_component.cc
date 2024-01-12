// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_feature_guarded_address_component.h"

#include "base/feature_list.h"

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

void FeatureGuardedAddressComponent::GetTypes(
    bool storable_only,
    FieldTypeSet* supported_types) const {
  if (!base::FeatureList::IsEnabled(*feature_)) {
    return;
  }
  AddressComponent::GetTypes(storable_only, supported_types);
}

}  // namespace autofill
