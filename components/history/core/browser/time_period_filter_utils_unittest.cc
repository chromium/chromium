// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/time_period_filter_utils.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/history/core/browser/features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace history {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

// Returns local midnight of the given date.
base::Time LocalDate(int year, int month, int day) {
  base::Time::Exploded exploded = {};
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_month = day;
  base::Time time;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded, &time));
  return time;
}

auto HasTimePeriod(TimePeriodOptionId id,
                   base::Time begin_time,
                   base::Time end_time) {
  return AllOf(Field(&TimePeriodOption::id, id),
               Field(&TimePeriodOption::begin_time, begin_time),
               Field(&TimePeriodOption::end_time, end_time));
}

auto HasIdAndLabel(TimePeriodOptionId id, int label_message_id) {
  return AllOf(Field(&TimePeriodOption::id, id),
               Field(&TimePeriodOption::label,
                     l10n_util::GetStringUTF16(label_message_id)));
}

class TimePeriodFilterUtilsTest : public ::testing::Test {
 public:
  TimePeriodFilterUtilsTest() {
    feature_list_.InitAndEnableFeature(kBrowsingHistoryFilterByTime);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST(TimePeriodFilterUtilsFeatureDisabledTest, ReturnsNoOptions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowsingHistoryFilterByTime);

  EXPECT_THAT(ComputeTimePeriodOptions(base::Time::Now()), IsEmpty());
}

TEST_F(TimePeriodFilterUtilsTest, OptionsHaveMatchingLabels) {
  EXPECT_THAT(
      ComputeTimePeriodOptions(base::Time::Now()),
      ElementsAre(
          HasIdAndLabel(TimePeriodOptionId::kAllTime,
                        IDS_HISTORY_TIME_PERIOD_FILTER_ALL_TIME),
          HasIdAndLabel(TimePeriodOptionId::kYesterday,
                        IDS_HISTORY_TIME_PERIOD_FILTER_YESTERDAY),
          HasIdAndLabel(TimePeriodOptionId::kEarlierThisWeek,
                        IDS_HISTORY_TIME_PERIOD_FILTER_EARLIER_THIS_WEEK),
          HasIdAndLabel(TimePeriodOptionId::kLastWeek,
                        IDS_HISTORY_TIME_PERIOD_FILTER_LAST_WEEK),
          HasIdAndLabel(TimePeriodOptionId::kTwoWeeksAgo,
                        IDS_HISTORY_TIME_PERIOD_FILTER_TWO_WEEKS_AGO)));
}

TEST_F(TimePeriodFilterUtilsTest, MidWeek) {
  // Wednesday, January 17th 2024, 15:30 local time.
  const base::Time now =
      LocalDate(2024, 1, 17) + base::Hours(15) + base::Minutes(30);

  EXPECT_THAT(
      ComputeTimePeriodOptions(now),
      ElementsAre(HasTimePeriod(TimePeriodOptionId::kAllTime, base::Time(),
                                base::Time()),
                  HasTimePeriod(TimePeriodOptionId::kYesterday,
                                LocalDate(2024, 1, 16), LocalDate(2024, 1, 17)),
                  HasTimePeriod(TimePeriodOptionId::kEarlierThisWeek,
                                LocalDate(2024, 1, 15), now),
                  HasTimePeriod(TimePeriodOptionId::kLastWeek,
                                LocalDate(2024, 1, 8), LocalDate(2024, 1, 15)),
                  HasTimePeriod(TimePeriodOptionId::kTwoWeeksAgo,
                                LocalDate(2024, 1, 1), LocalDate(2024, 1, 8))));
}

TEST_F(TimePeriodFilterUtilsTest, OnMonday) {
  // Monday, January 15th 2024, 10:00 local time. The week starts on Monday, so
  // "Earlier this week" only covers today so far and "Yesterday" falls into
  // last week.
  const base::Time now = LocalDate(2024, 1, 15) + base::Hours(10);

  EXPECT_THAT(
      ComputeTimePeriodOptions(now),
      ElementsAre(HasTimePeriod(TimePeriodOptionId::kAllTime, base::Time(),
                                base::Time()),
                  HasTimePeriod(TimePeriodOptionId::kYesterday,
                                LocalDate(2024, 1, 14), LocalDate(2024, 1, 15)),
                  HasTimePeriod(TimePeriodOptionId::kEarlierThisWeek,
                                LocalDate(2024, 1, 15), now),
                  HasTimePeriod(TimePeriodOptionId::kLastWeek,
                                LocalDate(2024, 1, 8), LocalDate(2024, 1, 15)),
                  HasTimePeriod(TimePeriodOptionId::kTwoWeeksAgo,
                                LocalDate(2024, 1, 1), LocalDate(2024, 1, 8))));
}

TEST_F(TimePeriodFilterUtilsTest, OnSunday) {
  // Sunday, January 21st 2024, 23:59 local time. Sunday is the last day of the
  // week, so "Earlier this week" spans Monday through now.
  const base::Time now =
      LocalDate(2024, 1, 21) + base::Hours(23) + base::Minutes(59);

  EXPECT_THAT(
      ComputeTimePeriodOptions(now),
      ElementsAre(HasTimePeriod(TimePeriodOptionId::kAllTime, base::Time(),
                                base::Time()),
                  HasTimePeriod(TimePeriodOptionId::kYesterday,
                                LocalDate(2024, 1, 20), LocalDate(2024, 1, 21)),
                  HasTimePeriod(TimePeriodOptionId::kEarlierThisWeek,
                                LocalDate(2024, 1, 15), now),
                  HasTimePeriod(TimePeriodOptionId::kLastWeek,
                                LocalDate(2024, 1, 8), LocalDate(2024, 1, 15)),
                  HasTimePeriod(TimePeriodOptionId::kTwoWeeksAgo,
                                LocalDate(2024, 1, 1), LocalDate(2024, 1, 8))));
}

TEST_F(TimePeriodFilterUtilsTest, AcrossMonthBoundary) {
  // Thursday, February 1st 2024. All ranges except "Yesterday"'s end fall into
  // January.
  const base::Time now = LocalDate(2024, 2, 1) + base::Hours(9);

  EXPECT_THAT(
      ComputeTimePeriodOptions(now),
      ElementsAre(
          HasTimePeriod(TimePeriodOptionId::kAllTime, base::Time(),
                        base::Time()),
          HasTimePeriod(TimePeriodOptionId::kYesterday, LocalDate(2024, 1, 31),
                        LocalDate(2024, 2, 1)),
          HasTimePeriod(TimePeriodOptionId::kEarlierThisWeek,
                        LocalDate(2024, 1, 29), now),
          HasTimePeriod(TimePeriodOptionId::kLastWeek, LocalDate(2024, 1, 22),
                        LocalDate(2024, 1, 29)),
          HasTimePeriod(TimePeriodOptionId::kTwoWeeksAgo,
                        LocalDate(2024, 1, 15), LocalDate(2024, 1, 22))));
}

}  // namespace

}  // namespace history
