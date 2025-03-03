// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/data_model_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::data_util {

void PrintTo(const Date& d, std::ostream* os) {
  *os << "Date{.year = " << d.year << ", .month = " << d.month
      << ", .day = " << d.day << "}";
}

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;

std::optional<Date> ParseDate(std::u16string_view date,
                              std::u16string_view format) {
  Date result;
  if (ParseDate(date, format, result)) {
    return result;
  }
  return std::nullopt;
}

TEST(AutofillDataModelUtils, IsValidDateFormat) {
  EXPECT_TRUE(IsValidDateFormat(u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateFormat(u"YYYY - MM - DD"));
  EXPECT_TRUE(IsValidDateFormat(u"YYYY-MM"));
  EXPECT_TRUE(IsValidDateFormat(u"YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"DD.MM.YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"DD . MM . YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"DD/MM/YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"DD / MM / YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"MM/DD/YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"MM / DD / YYYY"));
  EXPECT_TRUE(IsValidDateFormat(u"YYYYMMDD"));

  EXPECT_FALSE(IsValidDateFormat(u"DD/MM-YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u" DD/MM/YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DD/MM/YYYY "));
  EXPECT_FALSE(IsValidDateFormat(u"DD_MM_YYYY "));
  EXPECT_FALSE(IsValidDateFormat(u"YYDDMMYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYY"));
  EXPECT_FALSE(IsValidDateFormat(u"Y"));
  EXPECT_FALSE(IsValidDateFormat(u"MMM"));
  EXPECT_FALSE(IsValidDateFormat(u"DDD"));
  EXPECT_FALSE(IsValidDateFormat(u"yyyy"));
  EXPECT_FALSE(IsValidDateFormat(u"DD//YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"/MM/YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DD/MM/"));
  EXPECT_FALSE(IsValidDateFormat(u"/MM/"));
  EXPECT_FALSE(IsValidDateFormat(u"//"));
  EXPECT_FALSE(IsValidDateFormat(u"/"));
  EXPECT_FALSE(IsValidDateFormat(u"."));
  EXPECT_FALSE(IsValidDateFormat(u"-"));
  EXPECT_FALSE(IsValidDateFormat(u" / "));
  EXPECT_FALSE(IsValidDateFormat(u" . "));
  EXPECT_FALSE(IsValidDateFormat(u" - "));
  EXPECT_FALSE(IsValidDateFormat(u""));
  EXPECT_FALSE(IsValidDateFormat(u"_"));
  EXPECT_FALSE(IsValidDateFormat(u" _ "));
}

TEST(AutofillDataModelUtils, ParseDate) {
  EXPECT_EQ(ParseDate(u"2025-12-11", u"YYYY-MM-DD"), Date(2025, 12, 11));
  EXPECT_EQ(ParseDate(u"2025-02-01", u"YYYY-MM-DD"), Date(2025, 2, 1));
  EXPECT_EQ(ParseDate(u"25-02-01", u"YY-MM-DD"), Date(2025, 2, 1));
  EXPECT_EQ(ParseDate(u"00-02-01", u"YY-MM-DD"), Date(2000, 2, 1));
  EXPECT_EQ(ParseDate(u"20250201", u"YYYYMMDD"), Date(2025, 2, 1));
  EXPECT_EQ(ParseDate(u"250201", u"YYMMDD"), Date(2025, 2, 1));
  EXPECT_EQ(ParseDate(u"12/11/2025", u"MM/DD/YYYY"), Date(2025, 12, 11));
  EXPECT_EQ(ParseDate(u"11.12.2025", u"DD.MM.YYYY"), Date(2025, 12, 11));
  EXPECT_EQ(ParseDate(u"1.2.2025", u"D.M.YYYY"), Date(2025, 2, 1));
  EXPECT_EQ(ParseDate(u"11.12.99", u"D.M.YY"), Date(2099, 12, 11));
  EXPECT_EQ(ParseDate(u"1.2.00", u"D.M.YY"), Date(2000, 2, 1));

  EXPECT_EQ(ParseDate(u"09/2025", u"MM/YYYY"), Date(2025, 9, 0));
  EXPECT_EQ(ParseDate(u"9/25", u"M/YY"), Date(2025, 9, 0));

  EXPECT_EQ(ParseDate(u"28.02.", u"DD.MM."), Date(0, 2, 28));
  EXPECT_EQ(ParseDate(u"28.2.", u"D.M."), Date(0, 2, 28));

  EXPECT_EQ(ParseDate(u"2025", u"YYYY"), Date(2025, 0, 0));
  EXPECT_EQ(ParseDate(u"25", u"YY"), Date(2025, 0, 0));
  EXPECT_EQ(ParseDate(u"12", u"MM"), Date(0, 12, 0));
  EXPECT_EQ(ParseDate(u"12", u"M"), Date(0, 12, 0));
  EXPECT_EQ(ParseDate(u"2", u"M"), Date(0, 2, 0));
  EXPECT_EQ(ParseDate(u"11", u"DD"), Date(0, 0, 11));
  EXPECT_EQ(ParseDate(u"11", u"D"), Date(0, 0, 11));
  EXPECT_EQ(ParseDate(u"1", u"D"), Date(0, 0, 1));

  EXPECT_EQ(ParseDate(u"2025-12-11", u"YY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"25-12-11", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-02-11", u"YYYY-M-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-01", u"YYYY-MM-D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-2-11", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-1", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"02", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"01", u"D"), std::nullopt);

  {
    Date date;
    EXPECT_TRUE(ParseDate(u"2025-12-11", u"YYYY-MM-DD", date));
    EXPECT_EQ(date, Date(2025, 12, 11));
  }

  {
    Date date;
    EXPECT_FALSE(ParseDate(u"202512-10", u"YYYYMMDD", date));
    EXPECT_EQ(date, Date(2025, 12, 0));
  }

  {
    Date date;
    EXPECT_FALSE(ParseDate(u"2025-12-11", u"YYYY-MM", date));
    EXPECT_EQ(date, Date(2025, 12, 0));
  }

  {
    Date date;
    EXPECT_FALSE(ParseDate(u"2025-12", u"YYYY-MM-DD", date));
    EXPECT_EQ(date, Date(2025, 12, 0));
  }

  {
    Date date;
    EXPECT_FALSE(ParseDate(u"2025-12-10", u"YYYY-ZZ-DD", date));
    EXPECT_EQ(date, Date(2025, 0, 0));
  }
}

TEST(AutofillDataModelUtils, FormatDate) {
  EXPECT_EQ(FormatDate(Date(2025, 12, 11), u"YYYY-MM-DD"), u"2025-12-11");
  EXPECT_EQ(FormatDate(Date(2025, 12, 11), u"DD / MM / YY"), u"11 / 12 / 25");
  EXPECT_EQ(FormatDate(Date(2025, 2, 1), u"DD.MM.YYYY"), u"01.02.2025");
  EXPECT_EQ(FormatDate(Date(2000, 2, 1), u"DD.MM.YY"), u"01.02.00");
  EXPECT_EQ(FormatDate(Date(2000, 2, 1), u"D.M.YY"), u"1.2.00");
  EXPECT_EQ(FormatDate(Date(2000, 2, 1), u"NONSENSE D.M.Y"), u"NONSENSE 1.2.Y");
}

TEST(AutofillDataModelUtils, DateIsValid) {
  EXPECT_TRUE(Date(0, 0, 0).is_valid());
  EXPECT_TRUE(Date(2025, 1, 0).is_valid());
  EXPECT_TRUE(Date(2025, 1, 31).is_valid());
  EXPECT_FALSE(Date(2025, 1, 32).is_valid());
  EXPECT_TRUE(Date(2025, 2, 28).is_valid());
  EXPECT_FALSE(Date(2025, 2, 29).is_valid());
  EXPECT_TRUE(Date(1900, 2, 28).is_valid());
  EXPECT_FALSE(Date(1900, 2, 29).is_valid());
  EXPECT_TRUE(Date(2000, 2, 29).is_valid());
  EXPECT_FALSE(Date(2000, 2, 30).is_valid());
  EXPECT_TRUE(Date(2024, 2, 29).is_valid());
  EXPECT_FALSE(Date(2024, 2, 30).is_valid());
  EXPECT_TRUE(Date(2025, 3, 31).is_valid());
  EXPECT_FALSE(Date(2025, 3, 32).is_valid());
  EXPECT_TRUE(Date(2025, 4, 30).is_valid());
  EXPECT_FALSE(Date(2025, 4, 31).is_valid());
  EXPECT_TRUE(Date(2025, 5, 31).is_valid());
  EXPECT_FALSE(Date(2025, 5, 32).is_valid());
  EXPECT_TRUE(Date(2025, 6, 30).is_valid());
  EXPECT_FALSE(Date(2025, 6, 31).is_valid());
  EXPECT_TRUE(Date(2025, 7, 31).is_valid());
  EXPECT_FALSE(Date(2025, 7, 32).is_valid());
  EXPECT_TRUE(Date(2025, 8, 31).is_valid());
  EXPECT_FALSE(Date(2025, 8, 32).is_valid());
  EXPECT_TRUE(Date(2025, 9, 30).is_valid());
  EXPECT_FALSE(Date(2025, 9, 31).is_valid());
  EXPECT_TRUE(Date(2025, 10, 31).is_valid());
  EXPECT_FALSE(Date(2025, 10, 32).is_valid());
  EXPECT_TRUE(Date(2025, 11, 30).is_valid());
  EXPECT_FALSE(Date(2025, 11, 31).is_valid());
  EXPECT_TRUE(Date(2025, 12, 31).is_valid());
  EXPECT_FALSE(Date(2025, 12, 32).is_valid());
  EXPECT_FALSE(Date(2025, 13, 1).is_valid());
}

TEST(AutofillDataModelUtils, DateIsComplete) {
  EXPECT_TRUE(Date(2025, 12, 31).is_complete());
  EXPECT_FALSE(Date(2025, 12, 0).is_complete());
  EXPECT_FALSE(Date(2025, 0, 31).is_complete());
  EXPECT_FALSE(Date(0, 12, 31).is_complete());
}

}  // namespace
}  // namespace autofill::data_util
