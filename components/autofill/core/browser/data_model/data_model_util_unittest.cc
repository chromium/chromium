// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/data_model_util.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
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

TEST(AutofillDataModelUtils, ParseMonthFromString) {
  // Numeric months.
  EXPECT_EQ(ParseMonthFromString(u"1", ""), 1);
  EXPECT_EQ(ParseMonthFromString(u"01", "en-US"), 1);
  EXPECT_EQ(ParseMonthFromString(u"5", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"05", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"12", "en-US"), 12);
  EXPECT_EQ(ParseMonthFromString(u" 05 ", "en-US"), 5);

  // Exact and word-bounded full and abbreviated month names
  // (case/accent/period insensitive).
  EXPECT_EQ(ParseMonthFromString(u"January", "en-US"), 1);
  EXPECT_EQ(ParseMonthFromString(u"jan", "en-US"), 1);
  EXPECT_EQ(ParseMonthFromString(u"Jan.", "en-US"), 1);
  EXPECT_EQ(ParseMonthFromString(u"May", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"may", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"  May  ", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"05 - May", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"May (05)", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"Feb (2)", "en-US"), 2);
  EXPECT_EQ(ParseMonthFromString(u"janv", "fr-FR"), 1);
  EXPECT_EQ(ParseMonthFromString(u"janv.", "fr-FR"), 1);
  EXPECT_EQ(ParseMonthFromString(u"janvier", "fr-FR"), 1);
  EXPECT_EQ(ParseMonthFromString(u"FÉVRIER", "fr-FR"), 2);
  EXPECT_EQ(ParseMonthFromString(u"fevrier", "fr-FR"), 2);
  EXPECT_EQ(ParseMonthFromString(u"févr.", "fr-FR"), 2);
  EXPECT_EQ(ParseMonthFromString(u"08 - AOÛT", "fr-FR"), 8);
  EXPECT_EQ(ParseMonthFromString(u"12 - DECEMBRE", "fr-FR"), 12);

  // Substring collisions inside words and invalid inputs must not parse as a
  // month.
  EXPECT_EQ(ParseMonthFromString(u"MayTheForceBeWithYou", "en-US"),
            std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"SecretMay2026!", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"SmartCookie!", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"Janice", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"Augment", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"May2026", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"   ", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u".", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"-1", "en-US"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"13", "en-US"), std::nullopt);

  // When the killswitch is disabled, unanchored substring matching is restored.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillParseMonthFromStringByWordBoundary);
  EXPECT_EQ(ParseMonthFromString(u"MayTheForceBeWithYou", "en-US"), 5);
  EXPECT_EQ(ParseMonthFromString(u"SecretMay2026!", "en-US"), 5);
}

TEST(AutofillDataModelUtils, ParseMonthFromString_LocalizedDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillFixParseMonthFromString);

  EXPECT_EQ(ParseMonthFromString(u"05月", "ja"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"2025年11月", "ja"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"Wrzesień", "pl"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"Май", "ru"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"01 - Jan", "ja"), std::nullopt);
  EXPECT_EQ(ParseMonthFromString(u"January", ""), std::nullopt);
}

TEST(AutofillDataModelUtils, ParseMonthFromString_Localized) {
  base::test::ScopedFeatureList features{
      features::kAutofillFixParseMonthFromString};

  struct TestCase {
    std::u16string_view input;
    std::string_view locale;
    std::optional<int> expected;
  };
  // Keep as a table loop with `SCOPED_TRACE` rather than `TEST_P` to avoid
  // re-initializing `ScopedFeatureList` per test case while still reporting
  // each failing `(locale, input)` pair.
  constexpr auto kCases = std::to_array<TestCase>({
      // Bare numeric strings, whitespace, and bounds. "0" returns 0 (matching
      // `GetExpirationMonth(0)`), which is relied upon for 0-indexed `<select>`
      // controls where January is "0".
      {u"0", "", 0},
      {u"1", "", 1},
      {u" 05 ", "", 5},
      {u"12", "ja", 12},
      {u"-1", "en-US", std::nullopt},
      {u"13", "en-US", std::nullopt},
      {u"", "en-US", std::nullopt},
      {u"January", "", 1},
      {u"05 - May", "", 5},
      // Because the whole string is not a single number, the leading "06" is
      // ignored and the month name ("May") matches instead.
      {u"06 - May", "", 5},
      {u"MayTheForceBeWithYou", "", std::nullopt},
      // Word-boundary rejections preserved from the legacy path.
      {u"MayTheForceBeWithYou", "en-US", std::nullopt},
      {u"SecretMay2026!", "en-US", std::nullopt},
      {u"SmartCookie!", "en-US", std::nullopt},
      {u"Janice", "en-US", std::nullopt},
      {u"Augment", "en-US", std::nullopt},
      {u"May2026", "en-US", std::nullopt},
      // ICU's en-US abbreviation for September is "Sep" (not "Sept"), so
      // trailing word-boundary checks reject "Sep" inside "Sept."; en-GB
      // defines "Sept" in CLDR.
      {u"Sept.", "en-US", std::nullopt},
      {u"Sept.", "en-GB", 9},
      // Japanese (`ja`): suffix collisions, zero-padding, and fullwidth digits.
      {u"1月", "ja", 1},
      {u"01月", "ja", 1},
      {u"10月", "ja", 10},
      {u"11月", "ja", 11},
      {u"  12月  ", "ja", 12},
      {u"11 - 11月", "ja", 11},
      {u"13月 - 3月", "ja", 3},
      {u"2025年11月", "ja", 11},
      {u"０１月", "ja", 1},
      {u"１１月", "ja", 11},
      {u"00月", "ja", std::nullopt},
      {u"13月", "ja", std::nullopt},
      {u"１３月", "ja", std::nullopt},
      {u"101月", "ja", std::nullopt},
      {u"010月", "ja", std::nullopt},
      {u"０１０月", "ja", std::nullopt},
      // Unlike "2025年11月", a numeric date without a localized month marker
      // ("月") must not match as a named month.
      {u"2028/11", "ja", std::nullopt},
      // Chinese (`zh`): wide ("十一月"/"十二月") and numeric ("11月"/"12月").
      {u"一月", "zh", 1},
      {u"十月", "zh", 10},
      {u"十一月", "zh", 11},
      {u"十二月", "zh", 12},
      {u"11月", "zh", 11},
      {u"12月", "zh", 12},
      {u"08月", "zh", 8},
      {u"十三月", "zh", std::nullopt},
      // Korean (`ko`): suffix collisions ("11월"/"12월" vs "1월"/"2월").
      {u"1월", "ko", 1},
      {u"05월", "ko", 5},
      {u"11월", "ko", 11},
      {u"12월", "ko", 12},
      {u"13월", "ko", std::nullopt},
      // Vietnamese (`vi`): prefix collisions ("Tháng 10..12" vs "tháng 1").
      {u"Tháng 1", "vi", 1},
      {u"Tháng 10", "vi", 10},
      {u"Tháng 12", "vi", 12},
      {u"Tháng １２", "vi", 12},
      {u"Tháng 13 - Tháng 1", "vi", 1},
      {u"Thg 1", "vi", 1},
      {u"Thg 12", "vi", 12},
      {u"Tháng 13", "vi", std::nullopt},
      {u"Tháng １３", "vi", std::nullopt},
      // Polish (`pl`): mid-word "sie" (Aug=8) vs standalone "Wrzesień" (Sep=9).
      {u"Sierpień", "pl", 8},
      {u"Wrzesień", "pl", 9},
      {u"września", "pl", 9},
      {u"Grudzień", "pl", 12},
      // Russian (`ru`): standalone nominative ("Май") and format genitive.
      {u"Май", "ru", 5},
      {u"мая", "ru", 5},
      {u"Декабрь", "ru", 12},
      // Longest-match preference regardless of month order ("January" [7] and
      // "September" [9] both beat "May" [3]).
      {u"May - January", "en-US", 1},
      {u"May - September", "en-US", 9},
      // French (`fr`) & English (`en-US`): periods, NFD marks, and non-BMP
      // letters (𐐀 = U+10400, Deseret Capital Letter Long I, UTF-16 surrogate
      // pair).
      {u"févr. - 02", "fr", 2},
      {u"02 - févr.", "fr", 2},
      {u"de\u0301marche", "fr", std::nullopt},
      {u"Sep - 09", "en-US", 9},
      {u"𐐀May", "en-US", std::nullopt},
      {u"𐐀May - May", "en-US", 5},
      // English fallback when `app_locale` is non-English.
      {u"01 - Jan", "ja", 1},
      {u"11 - Nov", "ru", 11},
      {u"September", "pl", 9},
      {u"Sep.", "ko", 9},
      {u"NotAMonth", "en-US", std::nullopt},
      {u"NotAMonth", "ja", std::nullopt},
  });

  for (const TestCase& tc : kCases) {
    SCOPED_TRACE(testing::Message() << "locale=" << tc.locale << " input="
                                    << testing::PrintToString(tc.input));
    EXPECT_EQ(ParseMonthFromString(tc.input, tc.locale), tc.expected);
  }
  // Overlong/bogus locale tags (`icu::Locale::isBogus()`) should fall back to
  // English rather than matching ICU's "root" ("M01".."M12").
  EXPECT_EQ(ParseMonthFromString(u"January", std::string(300, 'x')), 1);
  EXPECT_EQ(ParseMonthFromString(u"M01", std::string(300, 'x')), std::nullopt);
}

}  // namespace
}  // namespace autofill::data_util
