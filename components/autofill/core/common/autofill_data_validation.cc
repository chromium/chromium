// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_data_validation.h"

#include "base/ranges/algorithm.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "url/gurl.h"

namespace autofill {

bool IsValidString(const std::string& str) {
  return str.size() <= kMaxStringLength;
}

bool IsValidString16(const std::u16string& str) {
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
         base::to_underlying(field.form_control_type()) >=
             base::to_underlying(FormControlType::kMinValue) &&
         base::to_underlying(field.form_control_type()) <=
             base::to_underlying(FormControlType::kMaxValue) &&
         IsValidString(field.autocomplete_attribute()) &&
         IsValidOptionVector(field.options());
}

bool IsValidFormData(const FormData& form) {
  return IsValidString16(form.name()) && IsValidGURL(form.url()) &&
         IsValidGURL(form.action()) && form.fields().size() <= kMaxListSize &&
         std::ranges::all_of(form.fields(), &IsValidFormFieldData);
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

bool IsValidOptionVector(const base::span<const SelectOption>& options) {
  return options.size() <= kMaxListSize &&
         std::ranges::all_of(options, &IsValidOption);
}

bool IsValidString16Vector(const base::span<const std::u16string>& strings) {
  return strings.size() <= kMaxListSize &&
         std::ranges::all_of(strings, &IsValidString16);
}

bool IsValidFormDataVector(const base::span<const FormData>& forms) {
  return forms.size() <= kMaxListSize &&
         std::ranges::all_of(forms, &IsValidFormData);
}

}  // namespace autofill
