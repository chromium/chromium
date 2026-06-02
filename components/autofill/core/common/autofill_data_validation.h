// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DATA_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DATA_VALIDATION_H_

#include <string>

#include "base/containers/span.h"

class GURL;

namespace autofill {

struct SelectOption;
class FormData;
class FormFieldData;
struct PasswordFormFillData;

// Functions to verify whether the objects passed to them satisfy basic sanity
// checks, including being capped to maximum constants.

bool IsValidString(std::string_view str);
bool IsValidString16(std::u16string_view str);
bool IsValidGURL(const GURL& url);
bool IsValidOption(const SelectOption& option);
bool IsValidFormFieldData(const FormFieldData& field);
bool IsValidFormFields(base::span<const FormFieldData> fields);
bool IsValidFormData(const FormData& form);
bool IsValidPasswordFormFillData(const PasswordFormFillData& form);
bool IsValidOptionVector(base::span<const SelectOption> options);
bool IsValidString16Vector(base::span<const std::u16string> strings);
bool IsValidFormDataVector(base::span<const FormData> forms);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DATA_VALIDATION_H_
