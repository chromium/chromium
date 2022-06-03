// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/rationalization_util.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {
namespace rationalization_util {

void RationalizePhoneNumberFields(
    std::vector<AutofillField*>& fields_in_section) {
  AutofillField* found_number_field = nullptr;
  AutofillField* found_number_field_second = nullptr;
  AutofillField* found_city_code_field = nullptr;
  AutofillField* found_country_code_field = nullptr;
  AutofillField* found_city_and_number_field = nullptr;
  AutofillField* found_whole_number_field = nullptr;
  bool phone_number_found = false;
  bool phone_number_separate_fields = false;
  // Iterate through all given fields. Iteration stops when it first finds a
  // valid set of fields that can compose a whole number. The |found_*| pointers
  // will be set to that set of fields when iteration finishes.
  for (AutofillField* field : fields_in_section) {
    if (!field->is_focusable)
      continue;
    ServerFieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
      case PHONE_BILLING_NUMBER:
        if (!found_number_field) {
          found_number_field = field;
          if (field->max_length < 5) {
            phone_number_separate_fields = true;
          } else {
            phone_number_found = true;
          }
          break;
        }
        // If the form has phone number separated into exchange and subscriber
        // number we mark both of them as number fields.
        // TODO(wuandy): A less hacky solution to have dedicated enum for
        // exchange and subscriber number.
        DCHECK(phone_number_separate_fields);
        DCHECK(!found_number_field_second);
        found_number_field_second = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_CITY_CODE:
      case PHONE_BILLING_CITY_CODE:
        if (!found_city_code_field)
          found_city_code_field = field;
        break;
      case PHONE_HOME_COUNTRY_CODE:
      case PHONE_BILLING_COUNTRY_CODE:
        if (!found_country_code_field)
          found_country_code_field = field;
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_BILLING_CITY_AND_NUMBER:
        DCHECK(!phone_number_found && !found_city_and_number_field);
        found_city_and_number_field = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_WHOLE_NUMBER:
      case PHONE_BILLING_WHOLE_NUMBER:
        DCHECK(!phone_number_found && !found_whole_number_field);
        found_whole_number_field = field;
        phone_number_found = true;
        break;
      default:
        break;
    }
    if (phone_number_found)
      break;
  }

  // The first number of found may be the whole number field, the
  // city and number field, or neither. But it cannot be both.
  DCHECK(!(found_whole_number_field && found_city_and_number_field));

  // Prefer to fill the first complete phone number found. The whole number
  // and city_and_number fields are only set if they represent the first
  // complete number found; otherwise, a complete number is present as
  // component input fields. These scenarios are mutually exclusive, so
  // clean up any inconsistencies.
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
  // compose the first valid phone number are set to not-NULL, specifically:
  // 1. |found_whole_number_field| is not NULL, other pointers set to NULL, or
  // 2. |found_city_and_number_field| is not NULL, |found_country_code_field| is
  //    probably not NULL, and other pointers set to NULL, or
  // 3. |found_city_code_field| and |found_number_field| are not NULL,
  //    |found_country_code_field| is probably not NULL, and other pointers are
  //    NULL.
  // 4. |found_city_code_field|, |found_number_field| and
  // |found_number_field_second|
  //    are not NULL, |found_country_code_field| is probably not NULL, and other
  //    pointers are NULL.

  // For all above cases, in the update pass, if one field is phone
  // number related but not one of the found fields from first pass, set their
  // |only_fill_when_focused| field to true.
  for (auto it = fields_in_section.begin(); it != fields_in_section.end();
       ++it) {
    AutofillField* field = *it;
    ServerFieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
      case PHONE_BILLING_NUMBER:
        if (field != found_number_field && field != found_number_field_second)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_CITY_CODE:
      case PHONE_BILLING_CITY_CODE:
        if (field != found_city_code_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_COUNTRY_CODE:
      case PHONE_BILLING_COUNTRY_CODE:
        if (field != found_country_code_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_BILLING_CITY_AND_NUMBER:
        if (field != found_city_and_number_field)
          field->set_only_fill_when_focused(true);
        break;
      case PHONE_HOME_WHOLE_NUMBER:
      case PHONE_BILLING_WHOLE_NUMBER:
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
