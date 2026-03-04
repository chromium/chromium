// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_format_string.h"

#include <string>

#include "base/check.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/proto/server.pb.h"

namespace autofill {

AutofillFormatString::AutofillFormatString() = default;

AutofillFormatString::AutofillFormatString(std::u16string v,
                                           FormatString_Type type)
    : value(std::move(v)), type(type) {
  DCHECK(IsValid(value, type));
}

AutofillFormatString::AutofillFormatString(const AutofillFormatString&) =
    default;

AutofillFormatString& AutofillFormatString::operator=(
    const AutofillFormatString&) = default;

AutofillFormatString::AutofillFormatString(AutofillFormatString&&) = default;

AutofillFormatString& AutofillFormatString::operator=(AutofillFormatString&&) =
    default;

AutofillFormatString::~AutofillFormatString() = default;

// static
bool AutofillFormatString::IsValid(std::u16string_view value,
                                   FormatString_Type type) {
  switch (type) {
    case FormatString_Type_DATE:
      return data_util::IsValidDateFormat(value);
    case FormatString_Type_AFFIX:
      return data_util::IsValidAffixFormat(value);
    case FormatString_Type_FLIGHT_NUMBER:
      return data_util::IsValidFlightNumberFormat(value);
    case FormatString_Type_ICU_DATE:
      // TODO(crbug.com/464004123): Add validation for ICU date format strings.
      return true;
  }
  // Graceful catch-all because the `type` may come from the server.
  return false;
}

}  // namespace autofill
