// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/birthdate.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

bool operator==(const Birthdate& a, const Birthdate& b) {
  return a.day_ == b.day_ && a.month_ == b.month_ && a.year_ == b.year_;
}

std::u16string Birthdate::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(AutofillType(type).group(), FieldTypeGroup::kBirthdateField);

  switch (type) {
    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_4_DIGIT_YEAR: {
      int value = GetRawInfoAsInt(type);
      return value != 0 ? base::NumberToString16(value) : std::u16string();
    }
    default:
      NOTREACHED();
      return std::u16string();
  }
}

int Birthdate::GetRawInfoAsInt(ServerFieldType type) const {
  switch (type) {
    case BIRTHDATE_DAY:
      return day_;
    case BIRTHDATE_MONTH:
      return month_;
    case BIRTHDATE_4_DIGIT_YEAR:
      return year_;
    default:
      NOTREACHED();
      return 0;
  }
}

void Birthdate::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                 const std::u16string& value,
                                                 VerificationStatus status) {
  DCHECK_EQ(AutofillType(type).group(), FieldTypeGroup::kBirthdateField);

  switch (type) {
    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_4_DIGIT_YEAR: {
      // If |value| is not a number, |StringToInt()| sets it to 0, which will
      // clear the field.
      int int_value;
      base::StringToInt(value, &int_value);
      SetRawInfoAsIntWithVerificationStatus(type, int_value, status);
      break;
    }
    default:
      NOTREACHED();
  }
}

void Birthdate::SetRawInfoAsIntWithVerificationStatus(
    ServerFieldType type,
    int value,
    VerificationStatus status) {
  auto ValueIfInRangeOrZero = [value](int lower_bound, int upper_bound) {
    return lower_bound <= value && value <= upper_bound ? value : 0;
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
    case BIRTHDATE_4_DIGIT_YEAR:
      year_ = ValueIfInRangeOrZero(1900, 9999);
      break;
    default:
      NOTREACHED();
  }
}

void Birthdate::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(BIRTHDATE_DAY);
  supported_types->insert(BIRTHDATE_MONTH);
  supported_types->insert(BIRTHDATE_4_DIGIT_YEAR);
}

}  // namespace autofill
