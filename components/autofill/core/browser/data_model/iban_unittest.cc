// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

constexpr char16_t kEllipsisOneDot[] = u"\u2022";
constexpr char16_t kEllipsisOneSpace[] = u"\u2006";

// A helper function gets the IBAN value returned by
// GetIdentifierStringForAutofillDisplay(), replaces the ellipsis ('\u2006')
// with a whitespace. If `is_value_masked` is true, replace oneDot ('\u2022')
// with '*'.
// This is useful to simplify the expectations in tests.
std::u16string GetHumanReadableIbanString(const Iban& iban,
                                          bool is_value_masked) {
  std::u16string identifierIbanValue =
      iban.GetIdentifierStringForAutofillDisplay(is_value_masked);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneSpace, u" ",
                     &identifierIbanValue);
  base::ReplaceChars(identifierIbanValue, kEllipsisOneDot, u"*",
                     &identifierIbanValue);
  return identifierIbanValue;
}

void SetPrefixAndSuffix(Iban& iban,
                        const std::u16string& prefix,
                        const std::u16string& suffix) {
  iban.set_prefix(prefix);
  iban.set_suffix(suffix);
}

TEST(IbanTest, AssignmentOperator) {
  // Creates two IBANs with different parameters.
  Iban iban_0 = test::GetLocalIban();
  Iban iban_1 = test::GetLocalIban2();
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
  Iban server_iban(Iban::InstrumentId(1234567));
  EXPECT_EQ(server_iban.record_type(), Iban::RecordType::kServerIban);
  EXPECT_EQ(1234567, server_iban.instrument_id());
}

TEST(IbanTest, GetMetadata) {
  Iban local_iban = test::GetLocalIban();
  local_iban.set_use_count(2);
  local_iban.set_use_date(base::Time::FromSecondsSinceUnixEpoch(25));
  PaymentsMetadata local_metadata = local_iban.GetMetadata();

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

// Verify that the value has been capitalized.
TEST(IbanTest, SetValue_Capitalization) {
  Iban iban;

  iban.set_value(u"be71096123456769");
  EXPECT_EQ(u"BE71096123456769", iban.value());
  EXPECT_EQ(u"BE", iban.prefix());
  EXPECT_EQ(u"6769", iban.suffix());

  iban.set_value(u"Br1500000000000010932840814P2");
  EXPECT_EQ(u"BR1500000000000010932840814P2", iban.value());
  EXPECT_EQ(u"BR", iban.prefix());
  EXPECT_EQ(u"14P2", iban.suffix());

  iban.set_value(u"fR7630006000011234567890189");
  EXPECT_EQ(u"FR7630006000011234567890189", iban.value());
  EXPECT_EQ(u"FR", iban.prefix());
  EXPECT_EQ(u"0189", iban.suffix());
}

// Verify that the prefix and suffix have been capitalized.
TEST(IbanTest, SetPrefixAndSuffix_Capitalization) {
  Iban iban;
  iban.set_prefix(u"be");
  iban.set_suffix(u"14p2");
  EXPECT_EQ(u"BE", iban.prefix());
  EXPECT_EQ(u"14P2", iban.suffix());

  iban.set_prefix(u"Be");
  iban.set_suffix(u"14p2");
  EXPECT_EQ(u"BE", iban.prefix());
  EXPECT_EQ(u"14P2", iban.suffix());
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

TEST(IbanTest, ValuePrefixAndSuffix) {
  Iban iban;
  iban.set_value(u"DE91100000000123456789");
  EXPECT_EQ(u"DE91100000000123456789", iban.value());
  EXPECT_EQ(u"DE", iban.prefix());
  EXPECT_EQ(u"6789", iban.suffix());

  iban.set_value(u"CH5604835012345678009");
  EXPECT_EQ(u"CH5604835012345678009", iban.value());
  EXPECT_EQ(u"CH", iban.prefix());
  EXPECT_EQ(u"8009", iban.suffix());
}

TEST(IbanTest, InvalidValuePrefixAndSuffix) {
  Iban iban;
  iban.set_value(u"DE1234567");
  EXPECT_EQ(u"", iban.value());
  EXPECT_EQ(u"", iban.prefix());
  EXPECT_EQ(u"", iban.suffix());

  iban.set_value(u"");
  EXPECT_EQ(u"", iban.value());
  EXPECT_EQ(u"", iban.prefix());
  EXPECT_EQ(u"", iban.suffix());
}

TEST(IbanTest, SetRawData) {
  Iban iban;

  // Verify RawInfo can be correctly set and read.
  iban.SetRawInfoWithVerificationStatus(IBAN_VALUE,
                                        u"DE91 1000 0000 0123 4567 89",
                                        VerificationStatus::kUserVerified);
  EXPECT_EQ(u"DE91100000000123456789", iban.GetRawInfo(IBAN_VALUE));
}

TEST(IbanTest, GetUserFacingValue_LocalIban) {
  // Verify each case of an IBAN ending in 1, 2, 3, and 4 unobfuscated
  // digits.
  Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  EXPECT_EQ(u"", GetHumanReadableIbanString(iban, /*is_value_masked=*/true));

  iban.set_value(u"CH5604835012345678009");

  EXPECT_EQ(u"CH **8009",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"CH56 0483 5012 3456 7800 9",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/false));

  iban.set_value(u"DE91100000000123456789");

  EXPECT_EQ(u"DE **6789",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/false));

  iban.set_value(u"GR9608100010000001234567890");

  EXPECT_EQ(u"GR **7890",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"GR96 0810 0010 0000 0123 4567 890",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/false));

  iban.set_value(u"PK70BANK0000123456789000");

  EXPECT_EQ(u"PK **9000",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/true));
  EXPECT_EQ(u"PK70 BANK 0000 1234 5678 9000",
            GetHumanReadableIbanString(iban, /*is_value_masked=*/false));
}

TEST(IbanTest, GetUserFacingValue_ServerIban_UnmaskNotAllowed) {
  Iban server_iban(Iban::InstrumentId(1234567));
  // Set the prefix and suffix of the server IBAN.
  SetPrefixAndSuffix(server_iban, u"FR76", u"0189");
  EXPECT_DEATH_IF_SUPPORTED(server_iban.GetIdentifierStringForAutofillDisplay(
                                /*is_value_masked=*/false),
                            "");
}

TEST(IbanTest, GetUserFacingValue_ServerIban_RegularPrefixAndSuffix) {
  Iban server_iban(Iban::InstrumentId(1234567));
  // Set the prefix and suffix of the server IBAN.
  SetPrefixAndSuffix(server_iban, u"FR", u"0189");
  EXPECT_EQ(u"FR **0189",
            GetHumanReadableIbanString(server_iban, /*is_value_masked=*/true));
}

TEST(IbanTest, GetUserFacingValue_ServerIban_EmptyPrefix) {
  // Set up a `server_iban` with empty prefix.
  Iban server_iban(Iban::InstrumentId(1234567));
  SetPrefixAndSuffix(server_iban, u"", u"0189");
  EXPECT_EQ(u" **0189",
            GetHumanReadableIbanString(server_iban, /*is_value_masked=*/true));
}

TEST(IbanTest, GetUserFacingValue_ServerIban_EmptySuffix) {
  // Set up a `server_iban` with empty suffix.
  Iban server_iban(Iban::InstrumentId(1234567));
  SetPrefixAndSuffix(server_iban, u"FR", u"");
  EXPECT_EQ(u"FR **",
            GetHumanReadableIbanString(server_iban, /*is_value_masked=*/true));
}

TEST(IbanTest, GetUserFacingValue_ServerIban_OtherLengthOfPrefixAndSuffix) {
  // Set the prefix and suffix of the server IBAN with length other than 4.
  Iban server_iban(Iban::InstrumentId(1234567));
  SetPrefixAndSuffix(server_iban, u"FR7", u"10189");
  EXPECT_EQ(u"FR7 **10189",
            GetHumanReadableIbanString(server_iban, /*is_value_masked=*/true));
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

TEST(IbanTest, GetCountryCode) {
  Iban iban;
  iban.set_value(u"ch5604835012345678009");

  EXPECT_EQ(iban.GetCountryCode(iban.value()), "CH");
  EXPECT_EQ(iban.GetCountryCode(), "CH");
}

TEST(IbanTest, IsIbanApplicableInCountry) {
  // Is an IBAN-supported country.
  EXPECT_TRUE(Iban::IsIbanApplicableInCountry("KW"));

  // Not an IBAN-supported country.
  EXPECT_FALSE(Iban::IsIbanApplicableInCountry("AB"));
}

TEST(IbanTest, GetIbanSupportedCountry) {
  // Is an IBAN-supported country.
  EXPECT_EQ(Iban::IbanSupportedCountry::kKW,
            Iban::GetIbanSupportedCountry("KW"));

  // Not an IBAN-supported country.
  EXPECT_EQ(Iban::IbanSupportedCountry::kUnsupported,
            Iban::GetIbanSupportedCountry("AB"));
}

// Test that `MatchesPrefixAndSuffix()` returns the expected outcome based
// on the prefix matching when the suffix matches already.
TEST(IbanTest, MatchesPrefixAndSuffix_Prefix) {
  const std::u16string prefix_1 = u"FR76";
  const std::u16string prefix_2 = u"FR75";
  const std::u16string prefix_1_shorter = u"FR7";
  const std::u16string prefix_1_longer = u"FR765";
  const std::u16string suffix = u"0189";
  Iban iban_1;
  Iban iban_2;
  SetPrefixAndSuffix(iban_1, prefix_1, suffix);
  SetPrefixAndSuffix(iban_2, prefix_2, suffix);

  // Should not match because prefix "FR76" != "FR75". Also, test both ways
  // because the order does not matter.
  EXPECT_FALSE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_FALSE(iban_2.MatchesPrefixAndSuffix(iban_1));

  iban_2 = iban_1;
  // Should match because the IBANs have equivalent data.
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));

  // Should match because "FR7" is still a prefix of "FR76".
  SetPrefixAndSuffix(iban_2, prefix_1_shorter, suffix);
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));

  // Should match because "FR76" is still a prefix of "FR765".
  SetPrefixAndSuffix(iban_2, prefix_1_longer, suffix);
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));
}

// Test that `MatchesPrefixAndSuffix()` returns the expected outcome based
// on the suffix matching when the prefix matches already.
TEST(IbanTest, MatchesPrefixAndSuffix_Suffix) {
  const std::u16string prefix = u"FR76";
  const std::u16string suffix_1 = u"0189";
  const std::u16string suffix_2 = u"1189";
  const std::u16string suffix_1_shorter = u"189";
  const std::u16string suffix_1_longer = u"00189";
  Iban iban_1;
  Iban iban_2;
  SetPrefixAndSuffix(iban_1, prefix, suffix_1);
  SetPrefixAndSuffix(iban_2, prefix, suffix_2);

  // Should not match because suffix "0189" != "1189".
  EXPECT_FALSE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_FALSE(iban_2.MatchesPrefixAndSuffix(iban_1));

  iban_2 = iban_1;
  // Should match because the IBANs have equivalent data.
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));

  // Should match because "189" is still a suffix of "0189".
  SetPrefixAndSuffix(iban_2, prefix, suffix_1_shorter);
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));

  // Should match because "0189" is still a suffix of "00189".
  SetPrefixAndSuffix(iban_2, prefix, suffix_1_longer);
  EXPECT_TRUE(iban_1.MatchesPrefixAndSuffix(iban_2));
  EXPECT_TRUE(iban_2.MatchesPrefixAndSuffix(iban_1));
}

// Test that `MatchesPrefixAndSuffix()` can match local IBANs to server
// IBANs correctly based on the prefix and suffix.
TEST(IbanTest, MatchesPrefixAndSuffix_AcrossTypes) {
  // `local_iban` and below server-based `server_iban` have the same prefix and
  // suffix.
  Iban local_iban(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  local_iban.set_value(u"CH56 0483 5012 3456 7800 9");
  Iban server_iban(Iban::InstrumentId(1234567));
  server_iban.set_prefix(u"CH");
  server_iban.set_suffix(u"8009");
  EXPECT_TRUE(local_iban.MatchesPrefixAndSuffix(server_iban));
  EXPECT_TRUE(server_iban.MatchesPrefixAndSuffix(local_iban));

  server_iban = test::GetServerIban2();
  EXPECT_FALSE(local_iban.MatchesPrefixAndSuffix(server_iban));
  EXPECT_FALSE(server_iban.MatchesPrefixAndSuffix(local_iban));
}

}  // namespace autofill
