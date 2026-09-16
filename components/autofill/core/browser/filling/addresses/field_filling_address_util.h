// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ADDRESSES_FIELD_FILLING_ADDRESS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ADDRESSES_FIELD_FILLING_ADDRESS_UTIL_H_

#include <stdint.h>

#include <string>

#include "components/autofill/core/browser/filling/field_filling_util.h"

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
FillingValueAndType GetFillingValueAndTypeForProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const AutofillType& field_type,
    const FormFieldData& field_data,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill = nullptr);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_ADDRESSES_FIELD_FILLING_ADDRESS_UTIL_H_
