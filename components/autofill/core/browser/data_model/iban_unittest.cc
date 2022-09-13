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
constexpr char16_t kEllipsisFourDotsAndOneSpace[] =
    u"\u2022\u2022\u2022\u2022\u2006";

// A helper function which constructs a string with |numberOfDots| of dots,
// followed by |revealingDigits|, and a trailing space if |addTrailingSpace|
// is true.
std::u16string AppendDotsAndRevealingDigits(
    int numberOfDots,
    const std::u16string& revealingDigits,
    bool addTrailingSpace) {
  std::u16string value;
  for (int i = 0; i < numberOfDots; ++i) {
    value.append(kEllipsisOneDot);
  }
  value.append(revealingDigits);

  if (addTrailingSpace)
    value.append(kEllipsisOneSpace);
  return value;
}

// A helper function to construct a string with
// |number_of_fully_obfuscated_groups| groups of '**** '.
std::u16string RepeatEllipsis(size_t number_of_fully_obfuscated_groups) {
  std::u16string ellipsis_value;
  ellipsis_value.reserve(sizeof(kEllipsisOneSpace) +
                         number_of_fully_obfuscated_groups *
                             sizeof(kEllipsisFourDotsAndOneSpace));
  ellipsis_value.append(kEllipsisOneSpace);
  for (size_t i = 0; i < number_of_fully_obfuscated_groups; ++i)
    ellipsis_value.append(kEllipsisFourDotsAndOneSpace);

  return ellipsis_value;
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
  iban.SetRawInfoWithVerificationStatus(
      IBAN_VALUE, u"DE91 1000 0000 0123 4567 89",
      structured_address::VerificationStatus::kUserVerified);
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89", iban.GetRawInfo(IBAN_VALUE));
}

// Verify that for all invalid IBAN values, empty identifier value will
// be returned.
TEST(IBANTest, GetObfuscatedStringForValue_InvalidIbanValue) {
  IBAN iban(base::GenerateGUID());
  iban.set_value(u"CH56-0483-5012-3456-7800-9999-9999-9999-9999");
  EXPECT_EQ(u"", iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"");
  EXPECT_EQ(u"", iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"CH5");
  EXPECT_EQ(u"", iban.GetIdentifierStringForAutofillDisplay());
}

TEST(IBANTest, GetObfuscatedStringForValue_ValidIbanValue) {
  // Verify each case of an IBAN ending in 1, 2, 3, and 4 unobfuscated
  // digits.
  IBAN iban(base::GenerateGUID());

  iban.set_value(u"CH56 0483 5012 3456 7800 9");
  std::u16string leading_digits = u"CH56";
  std::u16string trailing_digits =
      AppendDotsAndRevealingDigits(/*numberOfDots=*/1, u"800", true) + u"9";
  // Obfuscated value is: CH56 **** **** **** *800 9
  std::u16string expected =
      leading_digits + RepeatEllipsis(3) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"DE91 1000 0000 0123 4567 89");
  leading_digits = u"DE91";
  trailing_digits =
      AppendDotsAndRevealingDigits(/*numberOfDots=*/2, u"67", true) + u"89";
  // Obfuscated value is: DE91 **** **** **** **67 89
  expected = leading_digits + RepeatEllipsis(3) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"GR96 0810 0010 0000 0123 4567 890");
  leading_digits = u"GR96";
  trailing_digits =
      AppendDotsAndRevealingDigits(/*numberOfDots=*/3, u"7", true) + u"890";
  // Obfuscated value is: GR96 **** **** **** **** ***7 890
  expected = leading_digits + RepeatEllipsis(4) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"PK70 BANK 0000 1234 5678 9000");
  leading_digits = u"PK70";
  trailing_digits =
      AppendDotsAndRevealingDigits(/*numberOfDots=*/0, u"9000", false);
  // Obfuscated value is: PK70 **** **** **** **** 9000
  expected = leading_digits + RepeatEllipsis(4) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());
}

}  // namespace autofill
