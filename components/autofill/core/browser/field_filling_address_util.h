// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_ADDRESS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_ADDRESS_UTIL_H_

#include <stdint.h>

#include <string>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AddressNormalizer;
class AutofillProfile;
class AutofillType;
class FormFieldData;

// Returns the appropriate `profile` value based on `field_type` to fill
// into `field_data`, as well as the field type used to retrieve that value.
// Returns an empty string if no value could be found for the given `field_data`
// and `field_type`.
// TODO(crbug.com/40264633): Pass a `FieldType` instead of `AutofillType`.
std::pair<std::u16string, FieldType> GetFillingValueAndTypeForProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const AutofillType& field_type,
    const FormFieldData& field_data,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill = nullptr);

// Returns the phone number value for the given `field_max_length`. The
// returned value might be `number`, or `city_and_number`, or could possibly
// be a meaningful subset `number`, if that's appropriate for the field.
// TODO(crbug.com/40286472): Move to anonymous namespace in source file.
std::u16string GetPhoneNumberValueForInput(
    uint64_t field_max_length,
    const std::u16string& number,
    const std::u16string& city_and_number);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_ADDRESS_UTIL_H_
