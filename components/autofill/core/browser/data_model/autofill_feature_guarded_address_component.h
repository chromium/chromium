// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_FEATURE_GUARDED_ADDRESS_COMPONENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_FEATURE_GUARDED_ADDRESS_COMPONENT_H_

#include "base/feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// This class represents a type that is controlled by a feature flag. It
// overrides the SetValue method to prevent setting values to nodes for which
// the flag is turned off. It further prevents exposing disabled types as
// supported.
class FeatureGuardedAddressComponent : public AddressComponent {
 public:
  FeatureGuardedAddressComponent(raw_ptr<const base::Feature> feature,
                                 FieldType storage_type,
                                 SubcomponentsList children,
                                 unsigned int merge_mode);

  // AddressComponent overrides:
  void SetValue(std::u16string value, VerificationStatus status) override;
  void GetTypes(bool storable_only,
                FieldTypeSet* supported_types) const override;

 private:
  // Feature guarding the rollout of this address component.
  const raw_ptr<const base::Feature> feature_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_FEATURE_GUARDED_ADDRESS_COMPONENT_H_
