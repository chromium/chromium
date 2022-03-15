// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/birthdate.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

using structured_address::VerificationStatus;

bool operator==(const Birthdate& a, const Birthdate& b) {
  return a.day_ == b.day_ && a.month_ == b.month_ && a.year_ == b.year_;
}

std::u16string Birthdate::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(AutofillType(type).group(), FieldTypeGroup::kBirthdateField);

  auto ToStringOrEmpty = [](int n) {
    return n != 0 ? base::NumberToString16(n) : std::u16string();
  };

  switch (type) {
    case BIRTHDATE_DAY:
      return ToStringOrEmpty(day_);
    case BIRTHDATE_MONTH:
      return ToStringOrEmpty(month_);
    case BIRTHDATE_YEAR_4_DIGITS:
      return ToStringOrEmpty(year_);
    default:
      return std::u16string();
  }
}

void Birthdate::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                 const std::u16string& value,
                                                 VerificationStatus status) {
  DCHECK_EQ(AutofillType(type).group(), FieldTypeGroup::kBirthdateField);

  // If |value| is not a number, |StringToInt()| sets it to 0, which will clear
  // the field.
  int int_value;
  base::StringToInt(value, &int_value);

  auto ValueIfInRangeOrZero = [int_value](int lower_bound, int upper_bound) {
    return lower_bound <= int_value && int_value <= upper_bound ? int_value : 0;
  };
  // Set the appropriate field to |int_value| if it passes some rudimentary
  // validation. Otherwise clear it.
  switch (type) {
    case BIRTHDATE_DAY:
      day_ = ValueIfInRangeOrZero(1, 31);
      break;
    case BIRTHDATE_MONTH:
      month_ = ValueIfInRangeOrZero(1, 12);
      break;
    case BIRTHDATE_YEAR_4_DIGITS:
      year_ = ValueIfInRangeOrZero(1900, 9999);
      break;
    default:
      NOTREACHED();
  }
}

void Birthdate::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(BIRTHDATE_DAY);
  supported_types->insert(BIRTHDATE_MONTH);
  supported_types->insert(BIRTHDATE_YEAR_4_DIGITS);
}

}  // namespace autofill
