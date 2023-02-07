// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

constexpr char16_t kEllipsisOneDot[] = u"\u2022";
constexpr char16_t kEllipsisOneSpace[] = u"\u2006";

// A helper function gets the IBAN value returned by
// GetIdentifierStringForAutofillDisplay(), replaces the ellipsis ('\u2006')
// with a whitespace. If `is_value_masked` is true, replace oneDot ('\u2022')
// with '*'.
// This is useful to simplify the expectations in tests.
std::u16string GetIbanValueGroupedByFour(const IBAN& iban,
                                         bool is_value_masked) {
  std::u16string identifierIbanValue =
      iban.GetIdentifierStringForAutofillDisplay(is_value_masked);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneSpace, u" ",
                     &identifierIbanValue);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneDot, u"*",
                     &identifierIbanValue);
  return identifierIbanValue;
}

TEST(IBANTest, AssignmentOperator) {
  // Creates two IBANs with different parameters.
  std::string guid = base::GenerateGUID();
  IBAN iban_0;
  iban_0.set_guid(guid);
  iban_0.set_nickname(u"Nickname 0");
  iban_0.set_value(u"DE91 1000 0000 0123 4567 89");
  IBAN iban_1;
  guid = base::GenerateGUID();
  iban_1.set_guid(guid);
  iban_1.set_nickname(u"Nickname 1");
  iban_1.set_value(u"IE64 IRCE 9205 0112 3456 78");
  iban_1 = iban_0;

  EXPECT_EQ(iban_0, iban_1);
}

TEST(IBANTest, GetMetadata) {
  IBAN local_iban = test::GetIBAN();
  local_iban.set_use_count(2);
  local_iban.set_use_date(base::Time::FromDoubleT(25));
  AutofillMetadata local_metadata = local_iban.GetMetadata();

  EXPECT_EQ(local_iban.guid(), local_metadata.id);
  EXPECT_EQ(local_iban.use_count(), local_metadata.use_count);
  EXPECT_EQ(local_iban.use_date(), local_metadata.use_date);
}

// Verify that we set nickname with the processed string. We replace all tabs
// and newlines with whitespace, replace multiple spaces into a single one
// and trim leading/trailing whitespace.
TEST(IBANTest, SetNickname) {
  IBAN iban(base::GenerateGUID());

  // Normal input nickname.
  iban.set_nickname(u"My doctor's IBAN");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has leading and trailing whitespaces.
  iban.set_nickname(u"  My doctor's IBAN  ");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines.
  iban.set_nickname(u"\r\n My doctor's\nIBAN \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has tabs.
  iban.set_nickname(u" \tMy doctor's\t IBAN\t ");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines & whitespaces & tabs.
  iban.set_nickname(u"\n\t My doctor's \tIBAN \n \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines & tabs & multi spaces.
  iban.set_nickname(u"\n\t My doctor's    \tIBAN \n \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());
}

TEST(IBANTest, SetValue) {
  IBAN iban(base::GenerateGUID());

  // Input value.
  iban.set_value(u"DE91 1000 0000 0123 4567 89");
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89", iban.value());
}

TEST(IBANTest, SetRawData) {
  IBAN iban(base::GenerateGUID());

  // Verify RawInfo can be correctly set and read.
  iban.SetRawInfoWithVerificationStatus(IBAN_VALUE,
                                        u"DE91 1000 0000 0123 4567 89",
                                        VerificationStatus::kUserVerified);
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89", iban.GetRawInfo(IBAN_VALUE));
}

// Verify that for all invalid IBAN values, empty identifier value will
// be returned.
TEST(IBANTest, GetObfuscatedStringForValue_InvalidIbanValue) {
  IBAN iban(base::GenerateGUID());
  iban.set_value(u"CH56-0483-5012-3456-7800-9999-9999-9999-9999");
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));

  iban.set_value(u"");
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));

  iban.set_value(u"CH5");
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));
}

TEST(IBANTest, GetObfuscatedStringForValue_ValidIbanValue) {
  // Verify each case of an IBAN ending in 1, 2, 3, and 4 unobfuscated
  // digits.
  IBAN iban(base::GenerateGUID());

  iban.set_value(u"CH5604835012345678009");

  EXPECT_EQ(u"CH56 **** **** **** *800 9",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"CH56 0483 5012 3456 7800 9",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));

  iban.set_value(u"DE91100000000123456789");

  EXPECT_EQ(u"DE91 **** **** **** **67 89",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));

  iban.set_value(u"GR9608100010000001234567890");

  EXPECT_EQ(u"GR96 **** **** **** **** ***7 890",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"GR96 0810 0010 0000 0123 4567 890",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));

  iban.set_value(u"PK70BANK0000123456789000");

  EXPECT_EQ(u"PK70 **** **** **** **** 9000",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"PK70 BANK 0000 1234 5678 9000",
            GetIbanValueGroupedByFour(iban, /*is_value_masked=*/false));
}

}  // namespace autofill
