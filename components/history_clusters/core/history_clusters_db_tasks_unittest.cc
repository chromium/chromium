// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history_clusters/core/history_clusters_db_tasks.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

TEST(HistoryClustersDBTasksTest, BeginTimeCalculation) {
  struct TestData {
    base::Time::Exploded end_time_exploded;
    base::Time::Exploded expected_begin_time_exploded;
  } test_data[] = {
      // Times after 4PM yield a 4AM same day `begin_time`.
      // 2013-10-11 at 5:30PM and 15 seconds and 400 milliseconds.
      {
          {2013, 10, 6, 11, 17, 30, 15, 400},
          {2013, 10, 6, 11, 4, 0, 0, 0},
      },
      // Afternoon times before 4PM such as 2:00PM yield 4AM the day before.
      {
          {2013, 10, 6, 11, 14, 0, 0, 0},
          {2013, 10, 5, 10, 4, 0, 0, 0},
      },
      // Morning times like 10:15AM yield 4AM the day before.
      {
          {2013, 10, 6, 11, 10, 15, 0, 0},
          {2013, 10, 5, 10, 4, 0, 0, 0},
      },
      // Early morning times such as 2:10AM also yield 4AM the day before.
      // Just a sanity check here.
      {
          {2013, 10, 6, 11, 2, 10, 0, 0},
          {2013, 10, 5, 10, 4, 0, 0, 0},
      },
  };

  int i = 0;
  for (const auto& test_item : test_data) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d", i++));

    auto& test_case = test_item;

    ASSERT_TRUE(test_case.end_time_exploded.HasValidValues());
    base::Time end_time;
    ASSERT_TRUE(
        base::Time::FromLocalExploded(test_case.end_time_exploded, &end_time));

    base::Time begin_time =
        GetAnnotatedVisitsToCluster::GetBeginTimeOnDayBoundary(end_time);
    base::Time::Exploded begin_exploded;
    begin_time.LocalExplode(&begin_exploded);
    const auto& expected_begin = test_case.expected_begin_time_exploded;
    EXPECT_EQ(begin_exploded.year, expected_begin.year);
    EXPECT_EQ(begin_exploded.month, expected_begin.month);
    // We specifically ignore day-of-week, because it uses UTC, and we don't
    // actually care about which day of the week it is.
    EXPECT_EQ(begin_exploded.day_of_month, expected_begin.day_of_month);
    EXPECT_EQ(begin_exploded.hour, expected_begin.hour);
    EXPECT_EQ(begin_exploded.minute, expected_begin.minute);
    EXPECT_EQ(begin_exploded.second, expected_begin.second);
    EXPECT_EQ(begin_exploded.millisecond, expected_begin.millisecond);
  }
}

}  // namespace history_clusters
