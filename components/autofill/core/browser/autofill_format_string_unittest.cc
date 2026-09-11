// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_format_string.h"

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(AutofillFormatStringTest, IsValid) {
  EXPECT_TRUE(
      AutofillFormatString::IsValid(u"YYYY-MM-DD", FormatString_Type_DATE));
  EXPECT_TRUE(
      AutofillFormatString::IsValid(u"DD/MM/YYYY", FormatString_Type_DATE));
  EXPECT_FALSE(
      AutofillFormatString::IsValid(u"invalid", FormatString_Type_DATE));
  EXPECT_FALSE(AutofillFormatString::IsValid(u"N", FormatString_Type_DATE));
  EXPECT_FALSE(AutofillFormatString::IsValid(u"-4", FormatString_Type_DATE));

  EXPECT_TRUE(AutofillFormatString::IsValid(u"-4", FormatString_Type_AFFIX));
  EXPECT_TRUE(AutofillFormatString::IsValid(u"4", FormatString_Type_AFFIX));
  EXPECT_FALSE(
      AutofillFormatString::IsValid(u"invalid", FormatString_Type_AFFIX));

  EXPECT_TRUE(
      AutofillFormatString::IsValid(u"N", FormatString_Type_FLIGHT_NUMBER));
  EXPECT_TRUE(
      AutofillFormatString::IsValid(u"A", FormatString_Type_FLIGHT_NUMBER));
  EXPECT_TRUE(
      AutofillFormatString::IsValid(u"F", FormatString_Type_FLIGHT_NUMBER));
  EXPECT_FALSE(AutofillFormatString::IsValid(u"invalid",
                                             FormatString_Type_FLIGHT_NUMBER));
}

TEST(AutofillFormatStringTest, IsTypeCompatible) {
  // Date format strings are compatible with date field types.
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_DATE,
                                                     PASSPORT_EXPIRATION_DATE));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_DATE,
                                                     PASSPORT_ISSUE_DATE));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_DATE, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_ICU_DATE,
                                                     PASSPORT_EXPIRATION_DATE));

  // Date format strings are incompatible with non-date field types.
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(FormatString_Type_DATE,
                                                      PASSPORT_NUMBER));
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_DATE, FLIGHT_RESERVATION_FLIGHT_NUMBER));

  // Affix format strings are compatible with types where affixes are enabled.
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_AFFIX,
                                                     PASSPORT_NUMBER));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_AFFIX,
                                                     DRIVERS_LICENSE_NUMBER));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_AFFIX,
                                                     NATIONAL_ID_CARD_NUMBER));
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(FormatString_Type_AFFIX,
                                                     VEHICLE_VIN));

  // Affix format strings are incompatible with other types, notably dates.
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_AFFIX, PASSPORT_EXPIRATION_DATE));
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_AFFIX, FLIGHT_RESERVATION_FLIGHT_NUMBER));

  // Flight number format strings are only compatible with flight number.
  EXPECT_TRUE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_FLIGHT_NUMBER, FLIGHT_RESERVATION_FLIGHT_NUMBER));
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_FLIGHT_NUMBER, PASSPORT_EXPIRATION_DATE));
  EXPECT_FALSE(AutofillFormatString::IsTypeCompatible(
      FormatString_Type_FLIGHT_NUMBER, PASSPORT_NUMBER));
}

}  // namespace
}  // namespace autofill
