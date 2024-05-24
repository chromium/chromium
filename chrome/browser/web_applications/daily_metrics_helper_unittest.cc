// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/daily_metrics_helper.h"

#include <stdint.h>

#include <vector>

#include "base/numerics/clamped_math.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using testing::Contains;
using testing::Key;
using testing::Not;
using UkmEntry = ukm::builders::WebApp_DailyInteraction;

// EntryWithStartUrlAndInstallSource matcher is unable to refer to
// kInstallSourceNameHash directly.
constexpr auto kInstallSourceNameHash = UkmEntry::kInstallSourceNameHash;

MATCHER_P3(EntryWithStartUrlAndInstallSource,
           ukm_recorder,
           url,
           install_source,
           "") {
  return url == ukm_recorder->GetSourceForSourceId(arg->source_id)->url() &&
         install_source == arg->metrics.find(kInstallSourceNameHash)->second;
}

class DailyMetricsHelperTest : public WebAppTest {
 public:
  DailyMetricsHelperTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    WebAppTest::SetUp();

    SkipOriginCheckForTesting();
  }

  void FlushOldRecordsAndUpdate(DailyInteraction record) {
    web_app::FlushOldRecordsAndUpdate(record, profile());
  }

  void RecordSomethingTheNextDaySoItEmits() {
    task_environment()->FastForwardBy(base::Days(1));
    DailyInteraction record(GURL("http://this.should.not.be.emitted.com/"));
    FlushOldRecordsAndUpdate(record);
  }

  DailyMetricsHelperTest(const DailyMetricsHelperTest&) = delete;
  DailyMetricsHelperTest& operator=(const DailyMetricsHelperTest&) = delete;

  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

}  // namespace

TEST_F(DailyMetricsHelperTest, NothingEmittedForCallsInOneDay) {
  DailyInteraction record(GURL("http://some.url/"));

  FlushOldRecordsAndUpdate(record);
  FlushOldRecordsAndUpdate(record);
  FlushOldRecordsAndUpdate(record);

  EXPECT_EQ(ukm_recorder_.entries_count(), 0U);
}

TEST_F(DailyMetricsHelperTest, EmitsOldRecordsOnFirstCallNextDay) {
  DailyInteraction record1(GURL("http://some.url/1"));
  FlushOldRecordsAndUpdate(record1);

  RecordSomethingTheNextDaySoItEmits();

  EXPECT_EQ(ukm_recorder_.entries_count(), 1U);
  auto entries = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  ukm_recorder_.ExpectEntrySourceHasUrl(entries[0], record1.start_url);
}

TEST_F(DailyMetricsHelperTest, EmitsOncePerUrl) {
  {
    DailyInteraction record(GURL("http://some.url/1"));
    FlushOldRecordsAndUpdate(record);
    FlushOldRecordsAndUpdate(record);
    FlushOldRecordsAndUpdate(record);
  }
  {
    DailyInteraction record(GURL("http://some.url/2"));
    FlushOldRecordsAndUpdate(record);
  }

  RecordSomethingTheNextDaySoItEmits();

  EXPECT_EQ(ukm_recorder_.entries_count(), 2U);
}

TEST_F(DailyMetricsHelperTest, EmitsLatestValuePerUrl) {
  {
    DailyInteraction record1(GURL("http://some.url/1"));
    record1.install_source = 1;
    FlushOldRecordsAndUpdate(record1);
  }
  DailyInteraction record1(GURL("http://some.url/1"));
  record1.install_source = 2;
  FlushOldRecordsAndUpdate(record1);

  {
    DailyInteraction record2(GURL("http://some.url/2"));
    record2.install_source = 3;
    FlushOldRecordsAndUpdate(record2);
  }
  DailyInteraction record2(GURL("http://some.url/2"));
  record2.install_source = 4;
  FlushOldRecordsAndUpdate(record2);

  RecordSomethingTheNextDaySoItEmits();

  EXPECT_EQ(ukm_recorder_.entries_count(), 2U);
  auto entries = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 2U);
  ASSERT_THAT(entries, Contains(EntryWithStartUrlAndInstallSource(
                           &ukm_recorder_, record1.start_url, 2U)));
  ASSERT_THAT(entries, Contains(EntryWithStartUrlAndInstallSource(
                           &ukm_recorder_, record2.start_url, 4U)));
}

// Ensure last-recorded values are used for non-summed features.
TEST_F(DailyMetricsHelperTest, EmitsLatestValues) {
  // Record with default values.
  DailyInteraction record1(GURL("http://some.url/1"));
  FlushOldRecordsAndUpdate(record1);

  // Update with non-default values.
  DailyInteraction record2(GURL("http://some.url/1"));
  record2.installed = true;
  record2.install_source = 1;
  record2.effective_display_mode = 2;
  record2.promotable = true;
  FlushOldRecordsAndUpdate(record2);

  RecordSomethingTheNextDaySoItEmits();

  EXPECT_EQ(ukm_recorder_.entries_count(), 1U);
  auto* entry = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName)[0].get();
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstalledName, true);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kInstallSourceName, 1);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry,
                                                 UkmEntry::kDisplayModeName, 2);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kPromotableName, true);
}

TEST_F(DailyMetricsHelperTest, EmitsSumsForDurationsAndSessions) {
  // Default values are 0s
  DailyInteraction record(GURL("http://some.url/1"));
  FlushOldRecordsAndUpdate(record);

  record.foreground_duration = base::Hours(1);
  record.background_duration = base::Hours(2);
  record.num_sessions = 3;
  FlushOldRecordsAndUpdate(record);

  record.foreground_duration = base::Hours(4);
  record.background_duration = base::Hours(5);
  record.num_sessions = 6;
  FlushOldRecordsAndUpdate(record);

  RecordSomethingTheNextDaySoItEmits();

  ASSERT_EQ(ukm_recorder_.entries_count(), 1U);
  auto* entry = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName)[0].get();
  // 50 linear buckets per day, ie buckets of 1728 seconds,
  // 1+4 = 5 hours = 18000 seconds, bucketed into 10th bucket is 17280.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kForegroundDurationName, 17280);
  // 2+5 = 7 hours = 25200 seconds, bucketed into 14th bucket is 24192.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kBackgroundDurationName, 24192);
  // 3+6 = 9 sessions.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entry,
                                                 UkmEntry::kNumSessionsName, 9);
}

TEST_F(DailyMetricsHelperTest, EmitsClampedSumsForExtremeDurations) {
  DailyInteraction record(GURL("http://some.url/1"));
  record.foreground_duration = base::Seconds(1);
  record.background_duration = base::Hours(20);
  FlushOldRecordsAndUpdate(record);

  record.foreground_duration = base::Seconds(3);
  record.background_duration = base::Hours(15);
  FlushOldRecordsAndUpdate(record);

  RecordSomethingTheNextDaySoItEmits();

  ASSERT_EQ(ukm_recorder_.entries_count(), 1U);
  auto* entry = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName)[0].get();
  // 50 linear buckets per day, ie buckets of 1728 seconds,
  // 1+3 = 4 seconds, bucketed into 1st bucket so min value of 1.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kForegroundDurationName, 1);
  // 20+15 = 35 hours, clamped to 1 day = 86400 seconds, bucketed into 50th
  // bucket is 86400.
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, UkmEntry::kBackgroundDurationName, 86400);
}

TEST_F(DailyMetricsHelperTest, DoesNotEmitZeroDurationsOrSessions) {
  DailyInteraction record1(GURL("http://some.url/1"));
  FlushOldRecordsAndUpdate(record1);

  RecordSomethingTheNextDaySoItEmits();

  auto entries = ukm_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1U);
  auto* entry = entries[0].get();
  ukm_recorder_.ExpectEntrySourceHasUrl(entries[0], record1.start_url);
  ASSERT_THAT(entry->metrics,
              Not(Contains(Key(UkmEntry::kForegroundDurationNameHash))));
  ASSERT_THAT(entry->metrics,
              Not(Contains(Key(UkmEntry::kBackgroundDurationNameHash))));
  ASSERT_THAT(entry->metrics,
              Not(Contains(Key(UkmEntry::kNumSessionsNameHash))));
  // Sanity check that other metrics were in the entry.
  ASSERT_THAT(entry->metrics, Contains(Key(UkmEntry::kUsedNameHash)));
}

}  // namespace web_app
