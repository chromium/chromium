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
// checks, including being capped to the maximums defined by the constants
// above.
bool IsValidString(const std::string& str);
bool IsValidString16(const std::u16string& str);
bool IsValidGURL(const GURL& url);
bool IsValidOption(const SelectOption& option);
bool IsValidFormFieldData(const FormFieldData& field);
bool IsValidFormData(const FormData& form);
bool IsValidPasswordFormFillData(const PasswordFormFillData& form);
bool IsValidOptionVector(const base::span<const SelectOption>& options);
bool IsValidString16Vector(const base::span<const std::u16string>& strings);
bool IsValidFormDataVector(const base::span<const FormData>& forms);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DATA_VALIDATION_H_
