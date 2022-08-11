// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

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
  std::u16string leading_digits = u"CH";
  std::u16string trailing_digits = u"9";
  // Obfuscated value is: CH** **** **** **** **** 9
  std::u16string expected =
      leading_digits + iban.RepeatEllipsisForTesting(4) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"DE91 1000 0000 0123 4567 89");
  leading_digits = u"DE";
  trailing_digits = u"89";
  // Obfuscated value is: DE** **** **** **** **** 89
  expected =
      leading_digits + iban.RepeatEllipsisForTesting(4) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"GR96 0810 0010 0000 0123 4567 890");
  leading_digits = u"GR";
  trailing_digits = u"890";
  // Obfuscated value is: GR** **** **** **** **** **** 890
  expected =
      leading_digits + iban.RepeatEllipsisForTesting(5) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());

  iban.set_value(u"PK70 BANK 0000 1234 5678 9000");
  leading_digits = u"PK";
  trailing_digits = u"9000";
  // Obfuscated value is: PK** **** **** **** **** 9000
  expected =
      leading_digits + iban.RepeatEllipsisForTesting(4) + trailing_digits;

  EXPECT_EQ(expected, iban.GetIdentifierStringForAutofillDisplay());
}

}  // namespace autofill
