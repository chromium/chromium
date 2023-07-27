// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_

#include "components/autofill/core/browser/data_model/autofill_i18n_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"

namespace autofill {

enum class AutofillModelType {
  kAddressModel = 0,
  kNameModel = 1,
};

// Creates an instance of the hierarchy model corresponding to the
// given `AutofillModelType` in the provided country. All the nodes have
// empty values, except for the country node (if exist).
std::unique_ptr<I18nAddressComponent> CreateAddressComponentModel(
    AutofillModelType model_type,
    std::string_view country_code);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_
