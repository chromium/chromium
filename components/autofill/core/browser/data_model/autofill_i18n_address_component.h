// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_ADDRESS_COMPONENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_ADDRESS_COMPONENT_H_

#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

namespace autofill {

// Represents a country specific address hierarchy designed as part of the
// address model internationalization efforts. The address hierarchy for a
// specific country can be retrieved via autofill_i18n_api.h.
class I18nAddressComponent : public AddressComponent {
 public:
  I18nAddressComponent(
      ServerFieldType storage_type,
      std::vector<std::unique_ptr<I18nAddressComponent>> children,
      unsigned int merge_mode);
  ~I18nAddressComponent() override;

 private:
  const std::vector<std::unique_ptr<I18nAddressComponent>> children_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_ADDRESS_COMPONENT_H_
