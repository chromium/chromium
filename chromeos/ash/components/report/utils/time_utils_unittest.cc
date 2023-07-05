// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/time_utils.h"

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash::report::utils {

TEST(TimeUtilsTest, ConvertGmtToPt) {
  // Test case for converting to PT.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 1, 0, 28, 0, 0, 0}, &ts));

  // Set test clock to fake ts.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(ts);

  base::Time pt_ts = ConvertGmtToPt(&test_clock);

  // Validate that the converted time is 8 hours behind GMT time.
  base::TimeDelta expected_diff = base::Hours(-8);
  EXPECT_EQ(pt_ts - ts, expected_diff);
}

TEST(TimeUtilsTest, ConvertGmtToPtDaylightSavings) {
  // Test case for converting to PT during daylight savings.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 3, 0, 13, 0, 0, 0}, &ts));

  // Set test clock to fake ts.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(ts);

  base::Time pt_ts = ConvertGmtToPt(&test_clock);

  // Validate that the converted time is 7 hours behind GMT time.
  base::TimeDelta expected_diff = base::Hours(-7);
  EXPECT_EQ(pt_ts - ts, expected_diff);
}

TEST(TimeUtilsTest, ConvertGmtToPtUnknownTimeZone) {
  // Test case for converting to PT when the time zone is unknown.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 1, 0, 28, 0, 0, 0}, &ts));

  // Set test clock to fake ts.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(ts);

  // Mock TimeZone::createTimeZone() to return unknown time zone
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone(icu::UnicodeString("UnknownTimeZone")));

  base::Time pt_ts = ConvertGmtToPt(&test_clock);

  // Validate that the converted time is 8 hours behind GMT time.
  base::TimeDelta expected_diff = base::Hours(-8);
  EXPECT_EQ(pt_ts - ts, expected_diff);
}

TEST(TimeUtilsTest, GetPreviousMonth) {
  // Test case for getting the previous month.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 5, 0, 28, 0, 0, 0}, &ts));
  base::Time previous_month_ts = GetPreviousMonth(ts).value();

  // Validate that the previous month is April 2023
  base::Time expected_previous_month_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 4, 0, 1, 0, 0, 0},
                                          &expected_previous_month_ts));
  EXPECT_EQ(previous_month_ts, expected_previous_month_ts);
}

TEST(TimeUtilsTest, GetNextMonth) {
  // Test case for getting the next month.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 5, 0, 28, 0, 0, 0}, &ts));
  base::Time next_month_ts = GetNextMonth(ts).value();

  // Validate that the next month is June 2023
  base::Time expected_next_month_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 6, 0, 1, 0, 0, 0},
                                          &expected_next_month_ts));
  EXPECT_EQ(next_month_ts, expected_next_month_ts);
}

TEST(TimeUtilsTest, GetPreviousYear) {
  // Test case for getting the previous year.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 5, 0, 28, 0, 0, 0}, &ts));
  base::Time previous_year_ts = GetPreviousYear(ts).value();

  // Validate that the previous year is 2022
  base::Time expected_previous_year_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2022, 5, 0, 1, 0, 0, 0},
                                          &expected_previous_year_ts));
  EXPECT_EQ(previous_year_ts, expected_previous_year_ts);
}

TEST(TimeUtilsTest, GetPreviousMonthJanuary) {
  // Test case for getting the previous month when the current month is January.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 1, 0, 28, 0, 0, 0}, &ts));
  base::Time previous_month_ts = GetPreviousMonth(ts).value();

  // Validate that the previous month is December 2022
  base::Time expected_previous_month_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2022, 12, 0, 1, 0, 0, 0},
                                          &expected_previous_month_ts));
  EXPECT_EQ(previous_month_ts, expected_previous_month_ts);
}

TEST(TimeUtilsTest, GetNextMonthDecember) {
  // Test case for getting the next month when the current month is December.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 12, 0, 28, 0, 0, 0}, &ts));
  base::Time next_month_ts = GetNextMonth(ts).value();

  // Validate that the next month is January 2024
  base::Time expected_next_month_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2024, 1, 0, 1, 0, 0, 0},
                                          &expected_next_month_ts));
  EXPECT_EQ(next_month_ts, expected_next_month_ts);
}

TEST(TimeUtilsTest, GetPreviousYearJanuary) {
  // Test case for getting the previous year when the current year is January.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 1, 0, 28, 0, 0, 0}, &ts));
  base::Time previous_year_ts = GetPreviousYear(ts).value();

  // Validate that the previous year is 2022
  base::Time expected_previous_year_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded({2022, 1, 0, 1, 0, 0, 0},
                                          &expected_previous_year_ts));
  EXPECT_EQ(previous_year_ts, expected_previous_year_ts);
}

TEST(TimeUtilsTest, IsSameYearAndMonth) {
  // Test case for checking if two timestamps have the same year and month.
  base::Time ts1;
  base::Time ts2;
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 5, 0, 25, 0, 0, 0}, &ts1));
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 5, 0, 10, 0, 0, 0}, &ts2));

  // Validate that ts1 and ts2 have the same year and month
  EXPECT_TRUE(IsSameYearAndMonth(ts1, ts2));

  // Modify ts2 to have a different month
  EXPECT_TRUE(base::Time::FromUTCExploded({2023, 6, 0, 10, 0, 0, 0}, &ts2));
  EXPECT_FALSE(IsSameYearAndMonth(ts1, ts2));

  // Modify ts2 to have a different year
  EXPECT_TRUE(base::Time::FromUTCExploded({2024, 5, 0, 25, 0, 0, 0}, &ts2));
  EXPECT_FALSE(IsSameYearAndMonth(ts1, ts2));
}

TEST(TimeUtilsTest, FormatTimestampToMidnightGMTString) {
  // Test case for formatting a timestamp to midnight GMT string.
  base::Time ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2023, 5, 0, 25, 23, 59, 59, 999}, &ts));
  std::string formatted_ts = FormatTimestampToMidnightGMTString(ts);

  // Validate the formatted string
  std::string expected_formatted_ts = "2023-05-25 00:00:00.000 GMT";
  EXPECT_EQ(formatted_ts, expected_formatted_ts);
}

TEST(TimeUtilsTest, TimeToYYYYMMDDString) {
  // Test case for converting a time to YYYYMMDD string.
  base::Time ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2023, 5, 0, 25, 23, 59, 59, 999}, &ts));
  std::string formatted_ts = TimeToYYYYMMDDString(ts);

  // Validate the formatted string
  std::string expected_formatted_ts = "20230525";
  EXPECT_EQ(formatted_ts, expected_formatted_ts);
}

TEST(TimeUtilsTest, TimeToYYYYMMString) {
  // Test case for converting a time to YYYYMM string.
  base::Time ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2023, 5, 0, 25, 23, 59, 59, 999}, &ts));
  std::string formatted_ts = TimeToYYYYMMString(ts);

  // Validate the formatted string
  std::string expected_formatted_ts = "202305";
  EXPECT_EQ(formatted_ts, expected_formatted_ts);
}

}  // namespace ash::report::utils
