// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/card_value_validation.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

class CardValueValidationTest : public testing::Test {
 public:
  // Pins the current year to 2026, so that years from 2026 to 2046 are
  // plausible.
  CardValueValidationTest() { AdvanceToYear(2026); }

 protected:
  // Advances the mock clock to noon on June 15 of `year`, local time.
  void AdvanceToYear(int year) {
    base::Time target;
    CHECK(base::Time::FromLocalExploded(
        {.year = year, .month = 6, .day_of_month = 15, .hour = 12}, &target));
    task_environment_.AdvanceClock(target - base::Time::Now());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

bool IsPlausible(FieldType type,
                 std::u16string_view value,
                 bool is_select_element = false) {
  return IsPlausibleNonSensitiveCreditCardValue(type, value, is_select_element);
}

TEST_F(CardValueValidationTest, Names) {
  for (FieldType type :
       {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"Jane Doe"));
    EXPECT_TRUE(IsPlausible(type, u"José O'Brien-Núñez"));
    EXPECT_TRUE(IsPlausible(type, std::u16string(100, u'a')));
    EXPECT_FALSE(IsPlausible(type, u""));
    EXPECT_FALSE(IsPlausible(type, u"  "));
    EXPECT_FALSE(IsPlausible(type, u"4111111111111111"));
    EXPECT_FALSE(IsPlausible(type, u"Jane 4111"));
    EXPECT_FALSE(IsPlausible(type, u"123"));
    EXPECT_FALSE(IsPlausible(type, std::u16string(101, u'a')));
    // IsValidNameOnCard() returns true above MatchesRegex()'s 5000-character
    // limit, even with digits.
    EXPECT_FALSE(IsPlausible(type, std::u16string(5001, u'1')));
  }
}

TEST_F(CardValueValidationTest, Months) {
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"1"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"01"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"12"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u" 9 "));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u""));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"0"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"00"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"13"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"+1"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"-1"));
  // Month names aren't supported, since any value without digits, e.g. a
  // password, would pass as one.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Jan"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Dezember"));
  // Values with more than 2 digits, like CVCs, aren't months, even with
  // leading zeros.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"007"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"4111"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Jan 1"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"12/27"));
}

TEST_F(CardValueValidationTest, Years) {
  for (FieldType type :
       {CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"26"));
    EXPECT_TRUE(IsPlausible(type, u"46"));
    EXPECT_FALSE(IsPlausible(type, u"25"));
    EXPECT_FALSE(IsPlausible(type, u"47"));
    EXPECT_FALSE(IsPlausible(type, u"6"));
    EXPECT_FALSE(IsPlausible(type, u"026"));
    EXPECT_FALSE(IsPlausible(type, u"Jan"));
    // 4-digit years are only plausible in <select> elements, since CVCs can
    // have 4 digits.
    EXPECT_FALSE(IsPlausible(type, u"2030"));
    EXPECT_TRUE(IsPlausible(type, u"2026", /*is_select_element=*/true));
    EXPECT_TRUE(IsPlausible(type, u"2046", /*is_select_element=*/true));
    EXPECT_TRUE(IsPlausible(type, u"30", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"2025", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"2047", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"0026", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"20300", /*is_select_element=*/true));
  }
}

// Tests that 2-digit years count from the current year across centuries.
TEST_F(CardValueValidationTest, TwoDigitYearsAcrossCenturies) {
  AdvanceToYear(2090);
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"90"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"99"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"10"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"11"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"89"));
}

TEST_F(CardValueValidationTest, ExpirationDates) {
  for (FieldType type :
       {CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"12/27"));
    EXPECT_TRUE(IsPlausible(type, u"1/27"));
    EXPECT_TRUE(IsPlausible(type, u"12 / 27"));
    EXPECT_TRUE(IsPlausible(type, u"12-27"));
    EXPECT_TRUE(IsPlausible(type, u"12/2027"));
    // A separator is required, so that a 4-digit CVC can't pass as MMYY.
    EXPECT_FALSE(IsPlausible(type, u"1227"));
    EXPECT_FALSE(IsPlausible(type, u"122027"));
    EXPECT_FALSE(IsPlausible(type, u"00/27"));
    EXPECT_FALSE(IsPlausible(type, u"13/27"));
    EXPECT_FALSE(IsPlausible(type, u"012/27"));
    EXPECT_FALSE(IsPlausible(type, u"12/25"));
    EXPECT_FALSE(IsPlausible(type, u"12/2047"));
    EXPECT_FALSE(IsPlausible(type, u"12/27/30"));
    EXPECT_FALSE(IsPlausible(type, u"/27"));
    EXPECT_FALSE(IsPlausible(type, u"12/"));
  }
}

TEST_F(CardValueValidationTest, OtherTypes) {
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_NUMBER, u"4111111111111111"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_VERIFICATION_CODE, u"123"));
  // Android Autofill services don't fill card types.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_TYPE, u"Visa"));
  EXPECT_FALSE(IsPlausible(NAME_FULL, u"Jane Doe"));
  EXPECT_FALSE(IsPlausible(UNKNOWN_TYPE, u"12"));
}

}  // namespace
}  // namespace autofill
