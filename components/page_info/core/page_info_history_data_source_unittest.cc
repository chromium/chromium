// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/page_info_history_data_source.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace history {

class MockHistoryService : public HistoryService {
 public:
  MockHistoryService() = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLastVisitToHost,
              (const std::string& host,
               base::Time begin_time,
               base::Time end_time,
               GetLastVisitCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};

}  // namespace history

namespace page_info {

using testing::_;
using testing::Invoke;

base::Time kBase = base::Time::FromTimeT(1000);
base::Time kLastVisit = base::Time::FromTimeT(500);

base::CancelableTaskTracker::TaskId ReturnVisitedNever(
    const std::string& host,
    base::Time begin_time,
    base::Time end_time,
    history::HistoryService::GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryLastVisitResult result;
  result.success = true;
  result.last_visit = base::Time();
  std::move(callback).Run(result);
  return 0;
}

base::CancelableTaskTracker::TaskId ReturnVisitedBase(
    const std::string& host,
    base::Time begin_time,
    base::Time end_time,
    history::HistoryService::GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryLastVisitResult result;
  result.success = true;
  result.last_visit = kBase;
  std::move(callback).Run(result);
  return 0;
}

base::CancelableTaskTracker::TaskId ReturnLastVisited(
    const std::string& host,
    base::Time begin_time,
    base::Time end_time,
    history::HistoryService::GetLastVisitCallback callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryLastVisitResult result;
  result.success = true;
  result.last_visit = kLastVisit;
  std::move(callback).Run(result);
  return 0;
}

void CheckFormattedStringsForBaseTime(base::Time now) {
  // Tests all possible strings with minimal, middle and maximum values of the
  // range.
  base::Time midnight_today = now.LocalMidnight();

  base::Time today = midnight_today + base::Hours(2);
  base::Time today_min = midnight_today;
  base::Time today_max =
      (midnight_today + base::Days(1) + base::Hours(2)).LocalMidnight() -
      base::Seconds(1);

  base::Time yesterday = midnight_today - base::Days(1) + base::Hours(2);
  base::Time yesterday_min = midnight_today - base::Seconds(1);
  base::Time yesterday_max = yesterday.LocalMidnight();

  base::Time two_days_ago = midnight_today - base::Days(2) + base::Hours(2);
  base::Time two_days_ago_min = yesterday_max - base::Seconds(1);
  base::Time two_days_ago_max = two_days_ago.LocalMidnight();

  base::Time seven_days_ago = midnight_today - base::Days(7) + base::Hours(2);
  base::Time seven_days_ago_min =
      (midnight_today - base::Days(6) + base::Hours(2)).LocalMidnight() -
      base::Seconds(1);
  base::Time seven_days_ago_max = seven_days_ago.LocalMidnight();

  base::Time eight_days_ago =
      (midnight_today - base::Days(7)).LocalMidnight() - base::Seconds(1);

  EXPECT_EQ(PageInfoHistoryDataSource::FormatLastVisitedTimestamp(today, now),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_TODAY));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(today_min, now),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_TODAY));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(today_max, now),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_TODAY));

  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(yesterday, now),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_YESTERDAY));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(yesterday_min, now),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_YESTERDAY));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(yesterday_max, now),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_YESTERDAY));

  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(two_days_ago, now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 2));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(two_days_ago_min,
                                                            now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 2));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(two_days_ago_max,
                                                            now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 2));

  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(seven_days_ago,
                                                            now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 7));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(seven_days_ago_min,
                                                            now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 7));
  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(seven_days_ago_max,
                                                            now),
      l10n_util::GetStringFUTF16Int(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DAYS, 7));

  EXPECT_EQ(
      PageInfoHistoryDataSource::FormatLastVisitedTimestamp(eight_days_ago,
                                                            now),
      l10n_util::GetStringFUTF16(IDS_PAGE_INFO_HISTORY_LAST_VISIT_DATE,
                                 base::TimeFormatShortDate(eight_days_ago)));
}

class PageInfoHistoryDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    history_service_ =
        std::make_unique<testing::StrictMock<history::MockHistoryService>>();
    data_source_ = std::make_unique<PageInfoHistoryDataSource>(
        history_service_.get(), GURL("https://foo.com"));
  }

  history::MockHistoryService* history_service() {
    return history_service_.get();
  }
  PageInfoHistoryDataSource* data_source() { return data_source_.get(); }

 private:
  std::unique_ptr<history::MockHistoryService> history_service_;
  std::unique_ptr<PageInfoHistoryDataSource> data_source_;
};

TEST_F(PageInfoHistoryDataSourceTest, NoHistory) {
  // GetLastVisitToHost is called only once.
  EXPECT_CALL(*history_service(), GetLastVisitToHost(_, _, _, _, _))
      .WillOnce(Invoke(&ReturnVisitedNever));
  data_source()->GetLastVisitedTimestamp(base::BindOnce(
      [](std::optional<base::Time> time) { EXPECT_FALSE(time.has_value()); }));
}

TEST_F(PageInfoHistoryDataSourceTest, LastVisitedTimestamp) {
  // GetLastVisitToHost is called twice, once to get the latest visit (base) and
  // the second to get the visit before it (last visit).
  EXPECT_CALL(*history_service(), GetLastVisitToHost(_, _, _, _, _))
      .WillOnce(Invoke(&ReturnVisitedBase))
      .WillOnce(Invoke(&ReturnLastVisited));
  data_source()->GetLastVisitedTimestamp(
      base::BindOnce([](std::optional<base::Time> time) {
        EXPECT_TRUE(time.has_value());
        EXPECT_EQ(time.value(), kLastVisit);
      }));
}

TEST_F(PageInfoHistoryDataSourceTest, FormatTimestampString) {
  CheckFormattedStringsForBaseTime(base::Time::Now());

  // Test strings with the start of DST as the base time.
  base::Time start_of_dst;
  ASSERT_TRUE(base::Time::FromString("28 Mar 2021 10:30", &start_of_dst));
  CheckFormattedStringsForBaseTime(start_of_dst);

  // Test strings with the day after start of DST as the base time.
  base::Time after_start_of_dst;
  ASSERT_TRUE(base::Time::FromString("29 Mar 2021 10:30", &after_start_of_dst));
  CheckFormattedStringsForBaseTime(after_start_of_dst);

  // Test strings with the end of DST as the base time.
  base::Time end_of_dst;
  ASSERT_TRUE(base::Time::FromString("31 Oct 2021 10:30", &end_of_dst));
  CheckFormattedStringsForBaseTime(end_of_dst);

  // Test strings with 1 day after the end of DST as the base time.
  base::Time after_end_of_dst;
  ASSERT_TRUE(base::Time::FromString("1 Nov 2021 10:30", &after_end_of_dst));
  CheckFormattedStringsForBaseTime(after_end_of_dst);
}

}  // namespace page_info
