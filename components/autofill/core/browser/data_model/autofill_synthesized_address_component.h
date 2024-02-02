// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_SYNTHESIZED_ADDRESS_COMPONENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_SYNTHESIZED_ADDRESS_COMPONENT_H_

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// A synthesized address component is a type of node whose value is calculated
// from other address tree nodes (its constituents). While a synthesized address
// component is not directly part of the address hierarchy tree, its
// constituents are. Synthesized nodes cannot be stored, learned from form
// submissions, or viewed in settings.
class SynthesizedAddressComponent : public AddressComponent {
 public:
  SynthesizedAddressComponent(FieldType storage_type,
                              SubcomponentsList children,
                              unsigned int merge_mode);

  // AddressComponent overrides:
  void GetTypes(bool storable_only,
                FieldTypeSet* supported_types) const override;

  bool IsValueReadOnly() const override;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_SYNTHESIZED_ADDRESS_COMPONENT_H_
