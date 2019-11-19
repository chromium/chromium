// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_

#include <string>

#include "components/autofill/core/browser/field_types.h"

namespace base {
class ListValue;
class DictionaryValue;
}

namespace autofill {

class PersonalDataManager;

// Dictionary key for the field type.
extern const char kFieldTypeKey[];

// Dictionary key for the field length.
extern const char kFieldLengthKey[];

// Dictionary key for the field name.
extern const char kFieldNameKey[];

// Field name for autofill::NAME_FULL.
extern const char kFullNameField[];

// Field name for autofill::COMPANY_NAME.
extern const char kCompanyNameField[];

// Field name for autofill::ADDRESS_HOME_STREET_ADDRESS.
extern const char kAddressLineField[];

// Field name for autofill::ADDRESS_HOME_DEPENDENT_LOCALITY.
extern const char kDependentLocalityField[];

// Field name for autofill::ADDRESS_HOME_CITY.
extern const char kCityField[];

// Field name for autofill::ADDRESS_HOME_STATE.
extern const char kStateField[];

// Field name for autofill::ADDRESS_HOME_ZIP.
extern const char kPostalCodeField[];

// Field name for autofill::ADDRESS_HOME_SORTING_CODE.
extern const char kSortingCodeField[];

// Field name for autofill::ADDRESS_HOME_COUNTRY.
extern const char kCountryField[];

// AddressUiComponent::HINT_SHORT.
extern const bool kShortField;

// AddressUiComponent::HINT_LONG.
extern const bool kLongField;

// Converts a field type in string format as returned by
// autofill::GetAddressComponents into the appropriate autofill::ServerFieldType
// enum.
ServerFieldType GetFieldTypeFromString(const std::string& type);

// Fills |components| with the address UI components that should be used to
// input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display.
void GetAddressComponents(const std::string& country_code,
                          const std::string& ui_language_code,
                          base::ListValue* address_components,
                          std::string* components_language_code);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
