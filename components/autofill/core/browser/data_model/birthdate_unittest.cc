// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/birthdate.h"

#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

Birthdate CreateBirthdate(const std::u16string& day,
                          const std::u16string& month,
                          const std::u16string& year) {
  Birthdate birthdate;
  birthdate.SetRawInfo(BIRTHDATE_DAY, day);
  birthdate.SetRawInfo(BIRTHDATE_MONTH, month);
  birthdate.SetRawInfo(BIRTHDATE_4_DIGIT_YEAR, year);
  return birthdate;
}

void VerifyValues(const Birthdate& birthdate,
                  const std::u16string& day,
                  const std::u16string& month,
                  const std::u16string& year) {
  EXPECT_EQ(birthdate.GetRawInfo(BIRTHDATE_DAY), day);
  EXPECT_EQ(birthdate.GetRawInfo(BIRTHDATE_MONTH), month);
  EXPECT_EQ(birthdate.GetRawInfo(BIRTHDATE_4_DIGIT_YEAR), year);
}

// Expect that setting |field| to |value| clears the |field|. This is used to
// test invalid and empty values.
void SetFieldAndExpectEmpty(ServerFieldType field,
                            const std::u16string& value) {
  Birthdate birthdate = CreateBirthdate(u"14", u"3", u"1997");
  VerifyValues(birthdate, u"14", u"3", u"1997");
  birthdate.SetRawInfo(field, value);
  EXPECT_TRUE(birthdate.GetRawInfo(field).empty());
}

}  // anonymous namespace

// Tests writing and reading various valid birthdates.
TEST(BirthdateTest, ValidBirthdate) {
  Birthdate birthdate = CreateBirthdate(u"14", u"3", u"1997");
  VerifyValues(birthdate, u"14", u"3", u"1997");

  // Leading zeros are fine, but they are trimmed internally.
  birthdate = CreateBirthdate(u"07", u"06", u"1997");
  VerifyValues(birthdate, u"7", u"6", u"1997");

  // Leading and trailing spaces are fine too.
  birthdate = CreateBirthdate(u"24 ", u"  12", u" 2022 ");
  VerifyValues(birthdate, u"24", u"12", u"2022");
}

// Tests that invalid values clear the corresponding fields.
TEST(BirthdateTest, Validation) {
  SetFieldAndExpectEmpty(BIRTHDATE_DAY, u"42");
  SetFieldAndExpectEmpty(BIRTHDATE_DAY, u"NaN");
  SetFieldAndExpectEmpty(BIRTHDATE_MONTH, u"13");
  SetFieldAndExpectEmpty(BIRTHDATE_MONTH, u"a");
  SetFieldAndExpectEmpty(BIRTHDATE_4_DIGIT_YEAR, u"12345");
  SetFieldAndExpectEmpty(BIRTHDATE_4_DIGIT_YEAR, u"1234");
}

// Tests that empty values clear the corresponding fields.
TEST(BirthdateTest, Clear) {
  for (const ServerFieldType component : Birthdate::GetRawComponents()) {
    SetFieldAndExpectEmpty(component, u"");
  }
}

}  // namespace autofill
