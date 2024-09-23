// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/time_utils.h"

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
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

TEST(TimeUtilsTest, GetFirstActiveWeek) {
  system::FakeStatisticsProvider statistics_provider;
  system::StatisticsProvider::SetTestProvider(&statistics_provider);

  // Mocking the StatisticsProvider for testing.
  statistics_provider.SetMachineStatistic(system::kActivateDateKey, "2023-18");

  // Call the method under test
  auto result = GetFirstActiveWeek();
  EXPECT_TRUE(result.has_value());

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2023, 5, 0, 1, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, FirstMondayOfISONewYear2000) {
  // Call the method under test.
  std::optional<base::Time> result = FirstMondayOfISONewYear(2000);

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2000, 1, 0, 3, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, FirstMondayOfISONewYear2001) {
  // Call the method under test.
  std::optional<base::Time> result = FirstMondayOfISONewYear(2001);

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2001, 1, 0, 1, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, FirstMondayOfISONewYear2020) {
  // Call the method under test.
  std::optional<base::Time> result = FirstMondayOfISONewYear(2020);

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2019, 12, 1, 30, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, FirstMondayOfISONewYear2025) {
  // Call the method under test.
  std::optional<base::Time> result = FirstMondayOfISONewYear(2025);

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2024, 12, 1, 30, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, Iso8601DateWeekAsTime_2023_W18) {
  // Call method under test and calculate the expected result for comparison.
  auto result = Iso8601DateWeekAsTime(2023, 18);
  EXPECT_TRUE(result.has_value());
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2023, 5, 0, 1, 0, 0, 0, 0}, &expected_ts));
  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, Iso8601DateWeekAsTime_2026_W01) {
  // Call method under test and calculate the expected result for comparison.
  auto result = Iso8601DateWeekAsTime(2026, 1);
  EXPECT_TRUE(result.has_value());
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2025, 12, 1, 29, 0, 0, 0, 0}, &expected_ts));
  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, Iso8601DateWeekAsTime_2026_W53) {
  // Call method under test and calculate the expected result for comparison.
  auto result = Iso8601DateWeekAsTime(2026, 53);
  EXPECT_TRUE(result.has_value());
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2026, 12, 1, 28, 0, 0, 0, 0}, &expected_ts));
  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, InvalidIso8601DateWeekAsTime) {
  // Call the method under test
  auto result = Iso8601DateWeekAsTime(0, 0);
  EXPECT_FALSE(result.has_value());

  result = Iso8601DateWeekAsTime(-1, 0);
  EXPECT_FALSE(result.has_value());

  // Week of year is greater than 53.
  result = Iso8601DateWeekAsTime(2023, 54);
  EXPECT_FALSE(result.has_value());
}

TEST(TimeUtilsTest, Iso8601DateWeekAsTimeWeek53) {
  // Call the method under test
  // I.e Year 2020 has 53 weeks instead of 52.
  auto result = Iso8601DateWeekAsTime(2020, 53);
  EXPECT_TRUE(result.has_value());

  // Calculate the expected result for comparison.
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromUTCExploded({2020, 12, 0, 28, 0, 0, 0, 0}, &expected_ts));

  EXPECT_EQ(result.value(), expected_ts);
}

TEST(TimeUtilsTest, ConvertTimeToISO8601String) {
  // Call the method under test.
  // Cross validated against calculator: https://planetcalc.com/8540/
  struct {
    base::Time::Exploded exploded;
    std::string_view expected_yyyy_mm;
  } kTestCases[] = {
      {{2025, 1, 0, 1, 12, 30, 0, 0}, "2025-01"},
      {{2020, 1, 0, 1, 12, 30, 0, 0}, "2020-01"},
      {{2023, 1, 0, 1, 12, 30, 0, 0}, "2022-52"},
      {{2023, 12, 0, 30, 12, 30, 0, 0}, "2023-52"},
      {{2019, 12, 0, 30, 12, 30, 0, 0}, "2020-01"},
      {{2019, 8, 0, 25, 12, 30, 0, 0}, "2019-34"},
      {{2020, 8, 0, 25, 12, 30, 0, 0}, "2020-35"},
      {{2021, 8, 0, 25, 12, 30, 0, 0}, "2021-34"},
      {{2023, 11, 0, 25, 12, 30, 0, 0}, "2023-47"},
      {{2020, 12, 0, 27, 12, 30, 0, 0}, "2020-52"},
      {{2020, 12, 0, 28, 12, 30, 0, 0}, "2020-53"},
      {{2021, 1, 0, 4, 12, 30, 0, 0}, "2021-01"},
      // Edge Case: Returns 52 weeks, even though previous year has 53 weeks.
      // ISO calendar date to Gregorian calendar calculator outputs 2020-53.
      {{2021, 1, 0, 3, 12, 30, 0, 0}, "2020-52"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Case: " << test_case.expected_yyyy_mm);
    base::Time test_ts;
    EXPECT_TRUE(base::Time::FromUTCExploded(test_case.exploded, &test_ts));
    EXPECT_EQ(ConvertTimeToISO8601String(test_ts), test_case.expected_yyyy_mm);
  }
}

TEST(TimeUtilsTest, IsFirstActiveUnderFourMonthsAgo) {
  struct {
    base::Time::Exploded active_exploded;
    base::Time::Exploded first_active_exploded;
    bool expected_result;
  } kTestCases[] = {
      // Check when active and first active represent the same time.
      {{2021, 1, 0, 3, 12, 30, 0, 0}, {2021, 1, 0, 3, 12, 30, 0, 0}, true},
      // Check boundary case if first active under four months ago.
      {{2021, 1, 0, 31, 12, 30, 0, 0}, {2020, 10, 0, 1, 12, 30, 0, 0}, true},
      // Check outside four month boundary is false.
      {{2021, 1, 0, 31, 12, 30, 0, 0}, {2020, 9, 0, 1, 12, 30, 0, 0}, false},
  };

  for (const auto& test_case : kTestCases) {
    base::Time active_ts;
    base::Time first_active_ts;
    EXPECT_TRUE(
        base::Time::FromUTCExploded(test_case.active_exploded, &active_ts));
    EXPECT_TRUE(base::Time::FromUTCExploded(test_case.first_active_exploded,
                                            &first_active_ts));

    // 4 Months ago assuming max 31 days per month.
    int max_days_in_4_months = 31 * 4;
    EXPECT_EQ(IsFirstActiveUnderNDaysAgo(active_ts, first_active_ts,
                                         max_days_in_4_months),
              test_case.expected_result);
  }
}

TEST(TimeUtilsTest, IsFirstActiveUnderFiveWeeksAgo) {
  struct {
    base::Time::Exploded active_exploded;
    base::Time::Exploded first_active_exploded;
    bool expected_result;
  } kTestCases[] = {
      // Check when active and first active represent the same time.
      {{2021, 1, 0, 3, 12, 30, 0, 0}, {2021, 1, 0, 3, 12, 30, 0, 0}, true},
      // Check boundary case if first active under 5 weeks ago.
      {{2021, 2, 0, 4, 12, 30, 0, 0}, {2020, 12, 0, 31, 12, 30, 0, 0}, true},
      // Check outside four month boundary is false.
      {{2021, 1, 0, 31, 12, 30, 0, 0}, {2020, 9, 0, 1, 12, 30, 0, 0}, false},
  };

  for (const auto& test_case : kTestCases) {
    base::Time active_ts;
    base::Time first_active_ts;
    EXPECT_TRUE(
        base::Time::FromUTCExploded(test_case.active_exploded, &active_ts));
    EXPECT_TRUE(base::Time::FromUTCExploded(test_case.first_active_exploded,
                                            &first_active_ts));

    int max_days_in_5_weeks = 7 * 5;
    EXPECT_EQ(IsFirstActiveUnderNDaysAgo(active_ts, first_active_ts,
                                         max_days_in_5_weeks),
              test_case.expected_result);
  }
}

}  // namespace ash::report::utils
