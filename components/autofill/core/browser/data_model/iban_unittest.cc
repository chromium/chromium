// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/uuid.h"
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
std::u16string GetIbanValueGroupedByFour(const Iban& iban,
                                         bool is_value_masked) {
  std::u16string identifierIbanValue =
      iban.GetIdentifierStringForAutofillDisplay(is_value_masked);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneSpace, u" ",
                     &identifierIbanValue);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneDot, u"*",
                     &identifierIbanValue);
  return identifierIbanValue;
}

TEST(IbanTest, AssignmentOperator) {
  // Creates two IBANs with different parameters.
  Iban iban_0 = test::GetIban();
  Iban iban_1 = test::GetIban2();
  iban_1 = iban_0;

  EXPECT_EQ(iban_0, iban_1);
}

TEST(IbanTest, ConstructLocalIban) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  Iban local_iban{Iban::Guid(guid)};
  EXPECT_EQ(local_iban.record_type(), Iban::RecordType::kLocalIban);
  EXPECT_EQ(guid, local_iban.guid());
}

TEST(IbanTest, ConstructServerIban) {
  Iban server_iban(Iban::InstrumentId("1234567"));
  EXPECT_EQ(server_iban.record_type(), Iban::RecordType::kServerIban);
  EXPECT_EQ("1234567", server_iban.instrument_id());
}

TEST(IbanTest, GetMetadata) {
  Iban local_iban = test::GetIban();
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
TEST(IbanTest, SetNickname) {
  Iban iban;

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

TEST(IbanTest, SetValue) {
  Iban iban;

  // Verify middle whitespace was removed.
  iban.set_value(u"DE91 1000 0000 0123 4567 89");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());

  // Verify middle whitespace was removed.
  iban.set_value(u"DE911000      00000123 4567 89");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());

  // Verify leading whitespaces were removed.
  iban.set_value(u"  DE91100000000123 4567 89");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());

  // Verify trailing whitespaces were removed.
  iban.set_value(u"DE91100000000123 4567 89   ");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());
}

TEST(IbanTest, ValuePrefixSuffixAndLength) {
  Iban iban;
  iban.set_value(u"DE91100000000123456789");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());
  EXPECT_EQ(u"DE91", iban.prefix());
  EXPECT_EQ(u"6789", iban.suffix());
  EXPECT_EQ(22, iban.length());

  iban.set_value(u"CH5604835012345678009");
  EXPECT_EQ(u"CH5604835012345678009", iban.value());
  EXPECT_EQ(u"CH56", iban.prefix());
  EXPECT_EQ(u"8009", iban.suffix());
  EXPECT_EQ(21, iban.length());
}

TEST(IbanTest, InvalidValuePrefixSuffixAndLength) {
  Iban iban;
  iban.set_value(u"DE1234567");
  EXPECT_EQ(u"", iban.value());
  EXPECT_EQ(u"", iban.prefix());
  EXPECT_EQ(u"", iban.suffix());
  EXPECT_EQ(0, iban.length());

  iban.set_value(u"");
  EXPECT_EQ(u"", iban.value());
  EXPECT_EQ(u"", iban.prefix());
  EXPECT_EQ(u"", iban.suffix());
  EXPECT_EQ(0, iban.length());
}

TEST(IbanTest, SetRawData) {
  Iban iban;

  // Verify RawInfo can be correctly set and read.
  iban.SetRawInfoWithVerificationStatus(IBAN_VALUE,
                                        u"DE91 1000 0000 0123 4567 89",
                                        VerificationStatus::kUserVerified);
  EXPECT_EQ(u"DE91100000000123456789", iban.GetRawInfo(IBAN_VALUE));
}

TEST(IbanTest, GetUserFacingValue) {
  // Verify each case of an IBAN ending in 1, 2, 3, and 4 unobfuscated
  // digits.
  Iban iban;
  EXPECT_EQ(u"", GetIbanValueGroupedByFour(iban, /*is_value_masked=*/true));

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

TEST(IbanTest, ValidateIbanValue_ValidateOnLength) {
  // Empty string is an invalid IBAN value.
  EXPECT_FALSE(Iban::IsValid(u""));

  // The length of IBAN value is 15 which is invalid as valid IBAN value length
  // should be at least 16 digits and at most 33 digits long.
  EXPECT_FALSE(Iban::IsValid(u"AL1212341234123"));

  // The length of IBAN value is 35 which is invalid as valid IBAN value length
  // should be at least 16 digits and at most 33 digits long.
  EXPECT_FALSE(Iban::IsValid(u"AL121234123412341234123412341234123"));

  // Valid Belgium IBAN value with length of 16.
  EXPECT_TRUE(Iban::IsValid(u"BE71096123456769"));

  // Valid Russia IBAN value with length of 33.
  EXPECT_TRUE(Iban::IsValid(u"RU0204452560040702810412345678901"));
}

TEST(IbanTest, ValidateIbanValue_ModuloOnValue) {
  // The remainder of rearranged value of IBAN on division by 97 is 1
  EXPECT_TRUE(Iban::IsValid(u"GB82 WEST 1234 5698 7654 32"));

  // The remainder of rearranged value of IBAN on division by 97 is not 1.
  EXPECT_FALSE(Iban::IsValid(u"GB83 WEST 1234 5698 7654 32"));

  // The remainder of rearranged value of IBAN on division by 97 is not 1.
  EXPECT_FALSE(Iban::IsValid(u"AL36202111090000000001234567"));

  // The remainder of rearranged value of IBAN on division by 97 is not 1.
  EXPECT_FALSE(Iban::IsValid(u"DO21ACAU00000000000123456789"));
}

TEST(IbanTest, ValidateIbanValue_ValueWithCharacter) {
  // Valid Kuwait IBAN with multi whitespaces.
  EXPECT_TRUE(Iban::IsValid(u"KW81CB   KU00000000000   01234560101"));

  // Valid Kuwait IBAN with tab.
  EXPECT_TRUE(Iban::IsValid(u"KW81CBKU00000000000\t01234560101"));

  // Valid Kuwait IBAN with Carriage Return.
  EXPECT_TRUE(Iban::IsValid(u"KW81CBKU00000000000\r01234560101"));

  // Valid Kuwait IBAN with new line.
  EXPECT_TRUE(Iban::IsValid(u"KW81CBKU00000000000\n01234560101"));

  // Valid Kuwait IBAN with invalid special character.
  EXPECT_FALSE(Iban::IsValid(u"KW81CBKU00000000000*01234560101"));

  // Valid Kuwait IBAN with invalid special character.
  EXPECT_FALSE(Iban::IsValid(u"KW81CBKU000000#0000001234560101"));

  // Valid Kuwait IBAN with invalid special character.
  EXPECT_FALSE(Iban::IsValid(u"KW81CBKU000-0000000001234560101"));
}

TEST(IbanTest, ValidateIbanValue_ValidateOnRegexAndCountry) {
  // Valid Kuwait IBAN.
  EXPECT_TRUE(Iban::IsValid(u"KW81CBKU0000000000001234560101"));

  // Valid Kuwait IBAN with lower case character in the value.
  EXPECT_TRUE(Iban::IsValid(u"Kw81CBKU0000000000001234560101"));

  // The IBAN value does not match the IBAN value regex.
  EXPECT_FALSE(Iban::IsValid(u"KWA1CBKU0000000000001234560101"));

  // Invalid Kuwait IBAN with incorrect IBAN length.
  // KW16 will be converted into 203216, and the remainder on 97 is 1.
  EXPECT_FALSE(Iban::IsValid(u"KW1600000000000000000"));

  // The IBAN value country code is invalid.
  EXPECT_FALSE(Iban::IsValid(u"XXA1CBKU0000000000001234560101"));
}

TEST(IbanTest, IsIbanApplicableInCountry) {
  // Is an IBAN-supported country.
  EXPECT_TRUE(Iban::IsIbanApplicableInCountry("KW"));

  // Not an IBAN-supported country.
  EXPECT_FALSE(Iban::IsIbanApplicableInCountry("AB"));
}

}  // namespace autofill
