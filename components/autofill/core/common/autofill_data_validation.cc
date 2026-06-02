// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_data_validation.h"

#include <algorithm>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "url/gurl.h"

namespace autofill {

bool IsValidString(std::string_view str) {
  return str.size() <= kMaxStringLength;
}

bool IsValidString16(std::u16string_view str) {
  return str.size() <= kMaxStringLength;
}

bool IsValidGURL(const GURL& url) {
  return url.is_empty() || url.is_valid();
}

bool IsValidOption(const SelectOption& option) {
  return IsValidString16(option.text) && IsValidString16(option.value);
}

bool IsValidFormFieldData(const FormFieldData& field) {
  return IsValidString16(field.label()) && IsValidString16(field.name()) &&
         IsValidString16(field.value()) &&
         mojom::IsKnownEnumValue(field.form_control_type()) &&
         (!field.IsSelectElement() || field.max_length() == 0) &&
         IsValidString(field.autocomplete_attribute()) &&
         IsValidOptionVector(field.options());
}

bool IsValidFormFields(base::span<const FormFieldData> fields) {
  if (fields.size() > kMaxListSize ||
      !std::ranges::all_of(fields, &IsValidFormFieldData)) {
    // Return early to avoid the construction of the set if the fields are
    // invalid anyway.
    return false;
  }
  const auto unique_global_ids =
      base::MakeFlatSet<FieldGlobalId>(fields, {}, &FormFieldData::global_id);
  const bool all_global_ids_unique = unique_global_ids.size() == fields.size();
  base::UmaHistogramBoolean("Autofill.FormData.Fields.DuplicateGlobalIdFound",
                            !all_global_ids_unique);
  return all_global_ids_unique;
}

bool IsValidFormData(const FormData& form) {
  return IsValidString16(form.name()) && IsValidGURL(form.url()) &&
         IsValidGURL(form.action()) && IsValidFormFields(form.fields());
}

bool IsValidPasswordFormFillData(const PasswordFormFillData& form) {
  return IsValidGURL(form.url) &&
         IsValidString16(form.preferred_login.username_value) &&
         IsValidString16(form.preferred_login.password_value) &&
         IsValidString(form.preferred_login.realm) &&
         std::ranges::all_of(form.additional_logins, [](const auto& login) {
           return IsValidString16(login.username_value) &&
                  IsValidString16(login.password_value) &&
                  IsValidString(login.realm);
         });
}

bool IsValidOptionVector(base::span<const SelectOption> options) {
  return options.size() <= kMaxListSize &&
         std::ranges::all_of(options, &IsValidOption);
}

bool IsValidString16Vector(base::span<const std::u16string> strings) {
  return strings.size() <= kMaxListSize &&
         std::ranges::all_of(strings, &IsValidString16);
}

bool IsValidFormDataVector(base::span<const FormData> forms) {
  return forms.size() <= kMaxListSize &&
         std::ranges::all_of(forms, &IsValidFormData);
}

}  // namespace autofill
