// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/rationalization_util.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace rationalization_util {

void RationalizePhoneNumberFields(
    const std::vector<AutofillField*>& fields_in_section) {
  // A whole phone number can be structured in the following ways:
  // - whole number
  // - country code, city and number
  // - country code, city code, number field
  // - country code, city code, number field, second number field
  // In this function more or less anything ending in a local number field (see
  // `phone_number_found` below) is accepted as a valid phone number. Any
  // phone number fields after that number are labeled as
  // set_only_fill_when_focused(true) so that they don't get filled.
  AutofillField* found_number_field = nullptr;
  AutofillField* found_number_field_second = nullptr;
  AutofillField* found_city_code_field = nullptr;
  AutofillField* found_country_code_field = nullptr;
  AutofillField* found_city_and_number_field = nullptr;
  AutofillField* found_whole_number_field = nullptr;
  // The "number" here refers to the local part of a phone number (i.e.,
  // the part after a country code and a city code). It can be found as a
  // dedicated field or as part of a bigger scope (e.g. a whole number
  // field contains a "number"). The naming is sad but a relict from the past.
  bool phone_number_found = false;
  // Whether the number field (i.e. the local part) is split into two pieces.
  // This can be observed in the US, where 650 234-5678 would be a phone number
  // whose local parts are 234 and 5678.
  bool phone_number_separate_fields = false;
  // Iterate through all given fields. Iteration stops when it first finds a
  // field that indicates the end of a phone number (this can be the local part
  // of a phone number or a whole number). The |found_*| pointers will be set to
  // that set of fields when iteration finishes.
  for (AutofillField* field : fields_in_section) {
    if (!field->is_visible()) {
      continue;
    }
    FieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
        found_number_field = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_NUMBER_PREFIX:
        if (!found_number_field) {
          found_number_field = field;
          // phone_number_found is not set to true because the suffix needs to
          // be found first.
          phone_number_separate_fields = true;
        }
        break;
      case PHONE_HOME_NUMBER_SUFFIX:
        if (phone_number_separate_fields) {
          found_number_field_second = field;
          phone_number_found = true;
        }
        break;
      case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      case PHONE_HOME_CITY_CODE:
        if (!found_city_code_field)
          found_city_code_field = field;
        break;
      case PHONE_HOME_COUNTRY_CODE:
        if (!found_country_code_field)
          found_country_code_field = field;
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
        DCHECK(!phone_number_found && !found_city_and_number_field);
        found_city_and_number_field = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_WHOLE_NUMBER:
        DCHECK(!phone_number_found && !found_whole_number_field);
        found_whole_number_field = field;
        phone_number_found = true;
        break;
      default:
        break;
    }
    // Break here if the local part of a phone number was found because we
    // assume an order over the fields, where the local part comes last.
    if (phone_number_found)
      break;
  }

  // The first number of found may be the whole number field, the
  // city and number field, or neither. But it cannot be both.
  DCHECK(!(found_whole_number_field && found_city_and_number_field));

  // Prefer to fill the first complete phone number found. The whole_number
  // and city_and_number fields are only set if they occur before the local
  // part of a phone number. If we see the local part of a complete phone
  // number, we assume that the complete phone number is represented as a
  // sequence of fields (country code, city code, local part). These scenarios
  // are mutually exclusive, so clean up any inconsistencies.
  if (found_whole_number_field) {
    found_number_field = nullptr;
    found_number_field_second = nullptr;
    found_city_code_field = nullptr;
    found_country_code_field = nullptr;
  } else if (found_city_and_number_field) {
    found_number_field = nullptr;
    found_number_field_second = nullptr;
    found_city_code_field = nullptr;
  }

  // A second update pass.
  // At this point, either |phone_number_found| is false and we should do a
  // best-effort filling for the field whose types we have seen a first time.
  // Or |phone_number_found| is true and the pointers to the fields that
  // compose the first phone number are set to not-NULL. Specifically we hope
  // to find the following:
  // 1. |found_whole_number_field| is not NULL, other pointers set to NULL, or
  // 2. |found_city_and_number_field| is not NULL, |found_country_code_field| is
  //    probably not NULL, and other pointers set to NULL, or
  // 3. |found_city_code_field| and |found_number_field| are not NULL,
  //    |found_country_code_field| is probably not NULL, and other pointers are
  //    NULL.
  // 4. |found_city_code_field|, |found_number_field| and
  //    |found_number_field_second| are not NULL, |found_country_code_field| is
  //    probably not NULL, and other pointers are NULL.
  //
  // We currently don't guarantee these values. E.g. it is possible that
  // |found_city_code_field| is NULL but |found_number_field| is not NULL.

  // For all above cases, in the update pass, if one field is phone
  // number related but not one of the found fields from first pass, set their
  // |only_fill_when_focused| field to true.
  for (AutofillField* field : fields_in_section) {
    FieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
      case PHONE_HOME_NUMBER_PREFIX:
      case PHONE_HOME_NUMBER_SUFFIX:
        if (field != found_number_field && field != found_number_field_second)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_CITY_CODE:
      case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
        if (field != found_city_code_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_COUNTRY_CODE:
        if (field != found_country_code_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
        if (field != found_city_and_number_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_WHOLE_NUMBER:
        if (field != found_whole_number_field)
          field->set_only_fill_when_focused(true);
        break;
      default:
        break;
    }
  }
}

}  // namespace rationalization_util
}  // namespace autofill
