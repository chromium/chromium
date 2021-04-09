// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_

#include <string>

#include "components/autofill/core/browser/field_types.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"

namespace autofill {

class PersonalDataManager;

ServerFieldType AddressFieldToServerFieldType(
    ::i18n::addressinput::AddressField address_field);

// |address_components| is a 2D array for the address components in each line.
// Fills |address_components| with the address UI components that should be used
// to input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display.
void GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    std::vector<std::vector<::i18n::addressinput::AddressUiComponent>>*
        address_components,
    std::string* components_language_code);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
