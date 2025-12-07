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

// Tests the constraints defined in the documentation of IsValidDateFormat().
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
  EXPECT_TRUE(IsValidDateFormat(u"YYYY MM DD"));
  EXPECT_TRUE(IsValidDateFormat(u"YYYYMMDD"));
  EXPECT_TRUE(IsValidDateFormat(u"YY.M.D"));
  EXPECT_TRUE(IsValidDateFormat(u"D.M.YY"));
  EXPECT_TRUE(IsValidDateFormat(u"YY.M.D"));
  EXPECT_TRUE(IsValidDateFormat(u"YY-M"));
  EXPECT_TRUE(IsValidDateFormat(u"D/M"));

  EXPECT_FALSE(IsValidDateFormat(u"DD/MM-YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u" DD/MM/YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DD/MM/YYYY "));
  EXPECT_FALSE(IsValidDateFormat(u"DD_MM_YYYY "));
  EXPECT_FALSE(IsValidDateFormat(u"DDMM-YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DD-MMYYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYDDMMYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYYYMMD"));
  EXPECT_FALSE(IsValidDateFormat(u"DMMYYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYYYMDD"));
  EXPECT_FALSE(IsValidDateFormat(u"YYMMD"));
  EXPECT_FALSE(IsValidDateFormat(u"DMMYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYMDD"));
  EXPECT_FALSE(IsValidDateFormat(u"DDMYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYD"));
  EXPECT_FALSE(IsValidDateFormat(u"DYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYM"));
  EXPECT_FALSE(IsValidDateFormat(u"MYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYYYD"));
  EXPECT_FALSE(IsValidDateFormat(u"DYYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"YYYYM"));
  EXPECT_FALSE(IsValidDateFormat(u"MYYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"D.MYYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DM.YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"D.MYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DM.YY"));
  EXPECT_FALSE(IsValidDateFormat(u"DM"));
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

  // "*" and "+" are wildcards in ParseDate() but not valid date formats.
  EXPECT_FALSE(IsValidDateFormat(u"DD*MM*YYYY"));
  EXPECT_FALSE(IsValidDateFormat(u"DD+MM+YYYY"));
}

// Tests that ParseDate() extracts the right date from a format string.
// Format strings may contain wildcards for separators.
// Matches may be partial.
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

  EXPECT_EQ(ParseDate(u"0000-00-00", u"YYYY-MM-DD"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"00-00-00", u"YY-MM-DD"), Date(2000, 0, 0));

  EXPECT_EQ(ParseDate(u"09/2025", u"MM/YYYY"), Date(2025, 9, 0));
  EXPECT_EQ(ParseDate(u"9/25", u"M/YY"), Date(2025, 9, 0));

  EXPECT_EQ(ParseDate(u"23.02.", u"DD.MM."), Date(0, 2, 23));
  EXPECT_EQ(ParseDate(u"23.2.", u"D.M."), Date(0, 2, 23));

  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY*MM*DD"), Date(2025, 12, 10));
  EXPECT_EQ(ParseDate(u"20251210", u"YYYY*MM*DD"), Date(2025, 12, 10));
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY+MM+DD"), Date(2025, 12, 10));
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY+MM*DD"), Date(2025, 12, 10));
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY*MM+DD"), Date(2025, 12, 10));
  EXPECT_EQ(ParseDate(u"2025 / 12 / 10", u"YYYY*MM*DD"), Date(2025, 12, 10));

  EXPECT_EQ(ParseDate(u"2025", u"YYYY"), Date(2025, 0, 0));
  EXPECT_EQ(ParseDate(u"0001", u"YYYY"), Date(1, 0, 0));
  EXPECT_EQ(ParseDate(u"0000", u"YYYY"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"0123", u"YYYY"), Date(123, 0, 0));
  EXPECT_EQ(ParseDate(u"25", u"YY"), Date(2025, 0, 0));
  EXPECT_EQ(ParseDate(u"01", u"YY"), Date(2001, 0, 0));
  EXPECT_EQ(ParseDate(u"00", u"YY"), Date(2000, 0, 0));
  EXPECT_EQ(ParseDate(u"12", u"MM"), Date(0, 12, 0));
  EXPECT_EQ(ParseDate(u"00", u"MM"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"12", u"M"), Date(0, 12, 0));
  EXPECT_EQ(ParseDate(u"2", u"M"), Date(0, 2, 0));
  EXPECT_EQ(ParseDate(u"0", u"M"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"11", u"DD"), Date(0, 0, 11));
  EXPECT_EQ(ParseDate(u"11", u"D"), Date(0, 0, 11));
  EXPECT_EQ(ParseDate(u"00", u"DD"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"1", u"D"), Date(0, 0, 1));
  EXPECT_EQ(ParseDate(u"0", u"D"), Date(0, 0, 0));
  EXPECT_EQ(ParseDate(u"", u""), Date(0, 0, 0));

  EXPECT_EQ(ParseDate(u"", u"foo"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u""), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"foo", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"123", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"001", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"202", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"123", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"1", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"7", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"007", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"07", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"007", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"7", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"007", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"07", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"007", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"-1", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-11", u"YY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"25-12-11", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-02-11", u"YYYY-M-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-01", u"YYYY-MM-D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-2-11", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-1", u"YYYY-MM-DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"201", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"201", u"YYYY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025", u"YY"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"MM"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"M"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"DD"), std::nullopt);
  EXPECT_EQ(ParseDate(u"2025-12-10", u"D"), std::nullopt);
  EXPECT_EQ(ParseDate(u"20251210", u"YYYY+MM+DD"), std::nullopt);

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_TRUE(ParseDate(u"2025-12-11", u"YYYY-MM-DD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 11));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_TRUE(ParseDate(u"2025-12-10", u"YYYY+MM+DD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 10));
    EXPECT_EQ(std::u16string_view(separator), std::u16string_view(u"-"));
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_TRUE(ParseDate(u"2025-12-10", u"YYYY*MM*DD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 10));
    EXPECT_EQ(std::u16string_view(separator), std::u16string_view(u"-"));
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_TRUE(ParseDate(u"20251210", u"YYYY*MM*DD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 10));
    EXPECT_EQ(separator, std::u16string_view(u""));
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"20251210", u"YYYY+MM+DD", date, separator));
    EXPECT_EQ(date, Date(2025, 0, 0));
    EXPECT_EQ(separator, std::u16string_view(u""));
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"2025-12-10", u"YYYY-XX-DD", date, separator));
    EXPECT_EQ(date, Date(2025, 0, 0));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"202512-10", u"YYYYMMDD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 0));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"202521", u"YYYYMD", date, separator));
    EXPECT_EQ(date, Date(2025, 21, 0));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"2025-12-11", u"YYYY-MM", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 0));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"2025-12", u"YYYY-MM-DD", date, separator));
    EXPECT_EQ(date, Date(2025, 12, 0));
    EXPECT_EQ(separator, nullptr);
  }

  {
    Date date;
    const char16_t* separator = u"whatever";
    EXPECT_FALSE(ParseDate(u"2025-12-10", u"YYYY-ZZ-DD", date, separator));
    EXPECT_EQ(date, Date(2025, 0, 0));
    EXPECT_EQ(separator, nullptr);
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

TEST(AutofillDataModelUtils, IsValidDateForFormat) {
  // Zero-values not requested by the format are OK.
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 16), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 0), u"YYYY-MM"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 0, 16), u"YYYY-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(0, 12, 16), u"MM-DD"));
  // Same as above with a different format.
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 16), u"DD/MM/YYYY"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 0), u"MM/YYYY"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 0, 16), u"DD/YYYY"));
  EXPECT_TRUE(IsValidDateForFormat(Date(0, 12, 16), u"DD/MM"));

  // Zero-values requested by the format aren't formattable.
  EXPECT_FALSE(IsValidDateForFormat(Date(0, 12, 16), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 0, 16), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 12, 0), u"YYYY-MM-DD"));
  // Same as above with a different format.
  EXPECT_FALSE(IsValidDateForFormat(Date(0, 12, 16), u"DD/MM/YYYY"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 0, 16), u"DD/MM/YYYY"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 12, 0), u"DD/MM/YYYY"));

  // Maximum years.
  EXPECT_TRUE(IsValidDateForFormat(Date(9999, 12, 16), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(10000, 12, 16), u"YYYY-MM-DD"));

  // Maximum months.
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 20), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 13, 20), u"YYYY-MM-DD"));

  // Maximum days.
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 1, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 2, 28), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 2, 29), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(1900, 2, 28), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(1900, 2, 29), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2000, 2, 29), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2000, 2, 30), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2024, 2, 29), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2024, 2, 30), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 3, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 3, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 4, 30), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 4, 31), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 5, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 5, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 6, 30), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 6, 31), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 7, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 7, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 8, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 8, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 9, 30), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 9, 31), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 10, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 10, 32), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 11, 30), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 11, 31), u"YYYY-MM-DD"));
  EXPECT_TRUE(IsValidDateForFormat(Date(2025, 12, 31), u"YYYY-MM-DD"));
  EXPECT_FALSE(IsValidDateForFormat(Date(2025, 12, 32), u"YYYY-MM-DD"));
}

// Tests the constraints defined in the documentation of IsValidAffixFormat().
TEST(AutofillDataModelUtils, IsValidAffixFormat) {
  EXPECT_TRUE(IsValidAffixFormat(u"-8"));
  EXPECT_TRUE(IsValidAffixFormat(u"-7"));
  EXPECT_TRUE(IsValidAffixFormat(u"-6"));
  EXPECT_TRUE(IsValidAffixFormat(u"-5"));
  EXPECT_TRUE(IsValidAffixFormat(u"-4"));
  EXPECT_TRUE(IsValidAffixFormat(u"-3"));
  EXPECT_TRUE(IsValidAffixFormat(u"0"));
  EXPECT_TRUE(IsValidAffixFormat(u"0", /*exclude_full_value=*/false));
  EXPECT_TRUE(IsValidAffixFormat(u"3"));
  EXPECT_TRUE(IsValidAffixFormat(u"4"));
  EXPECT_TRUE(IsValidAffixFormat(u"5"));
  EXPECT_TRUE(IsValidAffixFormat(u"6"));
  EXPECT_TRUE(IsValidAffixFormat(u"7"));
  EXPECT_TRUE(IsValidAffixFormat(u"8"));

  EXPECT_FALSE(IsValidAffixFormat(u""));
  EXPECT_FALSE(IsValidAffixFormat(u"foo"));
  EXPECT_FALSE(IsValidAffixFormat(u"-100"));
  EXPECT_FALSE(IsValidAffixFormat(u"-9"));
  EXPECT_FALSE(IsValidAffixFormat(u"-2"));
  EXPECT_FALSE(IsValidAffixFormat(u"-1"));
  EXPECT_FALSE(IsValidAffixFormat(u"0", /*exclude_full_value=*/true));
  EXPECT_FALSE(IsValidAffixFormat(u"1"));
  EXPECT_FALSE(IsValidAffixFormat(u"2"));
  EXPECT_FALSE(IsValidAffixFormat(u"9"));
  EXPECT_FALSE(IsValidAffixFormat(u"100"));
}

TEST(AutofillDataModelUtilsTest, IsValidFlightNumberFormat) {
  EXPECT_TRUE(IsValidFlightNumberFormat(u"A"));
  EXPECT_TRUE(IsValidFlightNumberFormat(u"N"));
  EXPECT_TRUE(IsValidFlightNumberFormat(u"F"));

  EXPECT_FALSE(IsValidFlightNumberFormat(u"B"));
  EXPECT_FALSE(IsValidFlightNumberFormat(u"a"));
  EXPECT_FALSE(IsValidFlightNumberFormat(u"Aa"));
  EXPECT_FALSE(IsValidFlightNumberFormat(u"F", /*exclude_full_value=*/true));
}

TEST(AutofillDataModelUtilsTest, LocalizePattern_ShortMonthDay) {
  EXPECT_EQ(LocalizePattern(u"MMM d", "en_US"), u"MMM d");
  EXPECT_EQ(LocalizePattern(u"MMM d", "pl_PL"), u"d MMM");
  EXPECT_EQ(LocalizePattern(u"MMM d", "de_DE"), u"d. MMM");
}

TEST(AutofillDataModelUtilsTest, LocalizePattern_InvalidLocale) {
  EXPECT_EQ(LocalizePattern(u"MMM d", "thisisaninvalidlocale"), std::nullopt);
}

}  // namespace
}  // namespace autofill::data_util
