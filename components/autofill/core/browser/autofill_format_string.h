// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORMAT_STRING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORMAT_STRING_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

enum FormatString_Type : int;

// Describes formatting information for a field. Currently used only for
// filling Autofill AI data.
//
// Currently, the following kinds of format stings are supported:
// - Affix format strings: data_util::IsValidAffixFormat().
// - Date format strings: data_util::IsValidDateFormat().
// - Date format strings: ICU format.
// - Flight number format strings (data_util::IsValidFlightNumberFormat().
struct AutofillFormatString final {
  AutofillFormatString();
  AutofillFormatString(std::u16string value, FormatString_Type type);

  AutofillFormatString(const AutofillFormatString&);
  AutofillFormatString& operator=(const AutofillFormatString&);
  AutofillFormatString(AutofillFormatString&&);
  AutofillFormatString& operator=(AutofillFormatString&&);
  ~AutofillFormatString();

  static bool IsValid(std::u16string_view value, FormatString_Type type);
  static bool IsTypeCompatible(FormatString_Type format_type,
                               FieldType field_type);

  friend bool operator==(const AutofillFormatString&,
                         const AutofillFormatString&) = default;

  // The actual format string.
  std::u16string value;

  // Format strings can have different types: They can specify a date
  // format, an affix format, etc. See `FormatString_Type` for allowed values.
  FormatString_Type type{};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORMAT_STRING_H_
