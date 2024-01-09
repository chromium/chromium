// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/metrics/domain_diversity_reporter.h"

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// The interval between two scheduled computation tasks.
constexpr base::TimeDelta kScheduleInterval = base::Days(1);

// Pref name for the persistent timestamp of the last stats reporting.
// Should be in sync with similar name in the reporter's impl.
constexpr char kDomainDiversityReportingTimestamp[] =
    "domain_diversity.last_reporting_timestamp";
}  // namespace

namespace history {

class DomainDiversityReporterTest : public testing::Test {
 public:
  // TestClock uses a configurable time for testing.
  class TestClock : public base::Clock {
   public:
    explicit TestClock(base::Time time) : time_(time) {}
    base::Time Now() const override { return time_; }

    // Set the internal time.
    void SetTime(base::Time time) { time_ = time; }

   private:
    base::Time time_;
  };

  DomainDiversityReporterTest()
      : test_clock_(base::subtle::TimeNowIgnoringOverride()) {}

  DomainDiversityReporterTest(const DomainDiversityReporterTest&) = delete;
  DomainDiversityReporterTest& operator=(const DomainDiversityReporterTest&) =
      delete;

  ~DomainDiversityReporterTest() override = default;

  void SetUp() override {
    DomainDiversityReporter::RegisterProfilePrefs(pref_service_.registry());
    ASSERT_TRUE(history_dir_.CreateUniqueTempDir());

    // Creates HistoryService, but does not load it yet. Use LoadHistory() from
    // tests to control loading of HistoryService.
    history_service_ = std::make_unique<history::HistoryService>();

    // Sets the internal clock's current time to 10:00am. This avoids
    // issues in time arithmetic caused by uneven day lengths due to Daylight
    // Saving Time.
    test_clock_.SetTime(MidnightNDaysLater(test_clock_.Now(), 0) +
                        base::Hours(10));
  }

  void CreateDomainDiversityReporter() {
    // The domain diversity reporter will schedule a domain computation task
    // immediately upon creation. Therefore, the reporter should be created
    // after the last reporting time has been properly set.
    reporter_ = std::make_unique<DomainDiversityReporter>(
        history_service(), &pref_service_, &test_clock_);
  }

  // Wait for separate background task runner in HistoryService to complete
  // all tasks and then all the tasks on the current one to complete as well.
  void Wait() {
    history::BlockUntilHistoryProcessesPendingRequests(history_service());
  }

  // Fast-forward some time before Wait.
  void FastForwardAndWait(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    Wait();
  }

  bool LoadHistory() {
    if (!history_service_->Init(
            history::TestHistoryDatabaseParamsForPath(history_dir_.GetPath())))
      return false;
    history::BlockUntilHistoryProcessesPendingRequests(history_service());
    return true;
  }

  DomainDiversityReporter* reporter() const { return reporter_.get(); }
  const base::HistogramTester& histograms() const { return histogram_tester_; }
  history::HistoryService* history_service() { return history_service_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return &pref_service_;
  }
  TestClock& test_clock() { return test_clock_; }

 protected:
  // A `task_environment_` configured to MOCK_TIME so tests can
  // FastForwardAndWait() when waiting for a specific timeout (delayed task)
  // to fire. DomainDiversityReporter internally uses a `test_clock_` instead of
  // `task_environment_`'s clock because it needs to test very specific times
  // rather than just advance in deltas from Now().
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::ScopedTempDir history_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<DomainDiversityReporter> reporter_;

  // The mock clock used by DomainDiversity internally.
  TestClock test_clock_;
};

TEST_F(DomainDiversityReporterTest, HistoryNotLoaded) {
  EXPECT_FALSE(history_service()->backend_loaded());

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  // Since History is not yet loaded, there should be no histograms.
  histograms().ExpectTotalCount("History.DomainCount1Day_V2", 0);
  histograms().ExpectTotalCount("History.DomainCount7Day_V2", 0);
  histograms().ExpectTotalCount("History.DomainCount28Day_V2", 0);
  histograms().ExpectTotalCount("History.DomainCount1Day_V3", 0);
  histograms().ExpectTotalCount("History.DomainCount7Day_V3", 0);
  histograms().ExpectTotalCount("History.DomainCount28Day_V3", 0);

  // Load history. This should trigger reporter, via HistoryService observer.
  ASSERT_TRUE(LoadHistory());
  Wait();

  // No domains were visited, but there should be 7 samples. The last
  // reporting date, since it has never been set, was defaulted to epoch.
  histograms().ExpectUniqueSample("History.DomainCount1Day_V2", 0, 7);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V2", 0, 7);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V2", 0, 7);
  histograms().ExpectUniqueSample("History.DomainCount1Day_V3", 0, 7);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V3", 0, 7);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V3", 0, 7);
}

TEST_F(DomainDiversityReporterTest, HistoryLoaded) {
  EXPECT_FALSE(history_service()->backend_loaded());
  ASSERT_TRUE(LoadHistory());

  // Set the last reporting date to 1 day ago.
  prefs()->SetTime(kDomainDiversityReportingTimestamp,
                   MidnightNDaysLater(test_clock().Now(), -1));

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  // Since History is already loaded, there should be a sample reported.
  histograms().ExpectUniqueSample("History.DomainCount1Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount1Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V3", 0, 1);
}

TEST_F(DomainDiversityReporterTest, HostAddedSimple) {
  ASSERT_TRUE(LoadHistory());

  // The last report was 3 days ago.
  prefs()->SetTime(kDomainDiversityReportingTimestamp,
                   MidnightNDaysLater(test_clock().Now(), -3));

  // A domain was visited 2 days ago.
  base::Time two_days_ago = MidnightNDaysLater(test_clock().Now(), -2);

  history_service()->AddPage(GURL("http://www.google.com"), two_days_ago,
                             history::VisitSource::SOURCE_BROWSED);

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  // There are 3 samples for each histogram. One sample of DomainCount1Day,
  // two samples of DomainCount7Day and two samples of DomainCount28Day
  // should have a visit count of 1.
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 1, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 0, 2);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 1, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 0, 2);

  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 0, 1);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 0, 1);

  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 0, 1);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 0, 1);
}

TEST_F(DomainDiversityReporterTest, HostAddedLongAgo) {
  ASSERT_TRUE(LoadHistory());

  base::Time time_29_days_ago = MidnightNDaysLater(test_clock().Now(), -29);
  base::Time time_31_days_ago = MidnightNDaysLater(test_clock().Now(), -31);
  // The last report was 3 days ago.
  prefs()->SetTime(kDomainDiversityReportingTimestamp,
                   MidnightNDaysLater(test_clock().Now(), -3));

  // The visit occurring on the same day as the reporting time
  // will not be counted.
  history_service()->AddPage(GURL("http://www.google.com"), test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);

  // Visits occurring 29 days ago will affect some DomainCount28Day
  // whose spanning period begins 29 days ago or earlier.
  history_service()->AddPage(GURL("http://example1.com"), time_29_days_ago,
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://example2.com"), time_29_days_ago,
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://example3.com"), time_29_days_ago,
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://example4.com"), time_29_days_ago,
                             history::VisitSource::SOURCE_BROWSED);

  // Visit occurring 31 days ago will not show in this report, since the earlier
  // spanning period in the test case begins 30 days ago.
  history_service()->AddPage(GURL("http://example.com"), time_31_days_ago,
                             history::VisitSource::SOURCE_BROWSED);

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  histograms().ExpectUniqueSample("History.DomainCount1Day_V2", 0, 3);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V2", 0, 3);
  histograms().ExpectUniqueSample("History.DomainCount1Day_V3", 0, 3);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V3", 0, 3);

  // Two of the three DomainCount28Day samples should reflect the
  // 4 domain visits 29 days ago.
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 4, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 0, 1);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 4, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 0, 1);
}

TEST_F(DomainDiversityReporterTest, ScheduleNextDay) {
  // Test if the next domain metrics reporting task is scheduled every 24 hours
  ASSERT_TRUE(LoadHistory());

  // Last report was emitted 4 days ago. So the report emitted today
  // will emit one set of histogram values of each of the last 4 days.
  prefs()->SetTime(kDomainDiversityReportingTimestamp,
                   MidnightNDaysLater(test_clock().Now(), -4));
  history_service()->AddPage(GURL("http://www.google.com"),
                             MidnightNDaysLater(test_clock().Now(), -2),
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.example.com"),
                             MidnightNDaysLater(test_clock().Now(), -2),
                             history::VisitSource::SOURCE_BROWSED);

  // These visits are ignored in the initial DomainCount1Day values,
  // but will show in the one scheduled 24 hours later.
  history_service()->AddPage(GURL("http://www.example1.com"),
                             test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.example2.com"),
                             test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.example3.com"),
                             test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.google.com"), test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);

  // These visits are included in the DomainCount7Day and DomainCount28Day
  // values of the first report, but will expire for the second report.
  history_service()->AddPage(GURL("http://www.visited-7-days-ago1.com"),
                             MidnightNDaysLater(test_clock().Now(), -7),
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://www.visited-28-days-ago.com"),
                             MidnightNDaysLater(test_clock().Now(), -28),
                             history::VisitSource::SOURCE_BROWSED);

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  // Two domains visited two days ago.
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 2, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 0, 3);
  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 3, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 2, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 4, 2);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 2, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 0, 3);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 3, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 2, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 4, 2);

  test_clock().SetTime(MidnightNDaysLater(test_clock().Now(), 1) +
                       base::Hours(10));
  FastForwardAndWait(kScheduleInterval);  // fast-forward 24 hours

  // The new report will include the four domain visits on the last
  // repoting date.
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 4, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 2, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V2", 0, 3);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 4, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 2, 1);
  histograms().ExpectBucketCount("History.DomainCount1Day_V3", 0, 3);

  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 5, 1);
  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V2", 3, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 5, 1);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 1, 2);
  histograms().ExpectBucketCount("History.DomainCount7Day_V3", 3, 2);

  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 6, 1);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 2, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V2", 4, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 6, 1);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 2, 2);
  histograms().ExpectBucketCount("History.DomainCount28Day_V3", 4, 2);
}

TEST_F(DomainDiversityReporterTest, SaveTimestampInPreference) {
  ASSERT_TRUE(LoadHistory());
  base::Time last_midnight = MidnightNDaysLater(test_clock().Now(), -1);
  prefs()->SetTime(kDomainDiversityReportingTimestamp, last_midnight);
  EXPECT_EQ(last_midnight,
            prefs()->GetTime(kDomainDiversityReportingTimestamp));

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  // Reporter should have updated the pref to the time of the request.
  EXPECT_EQ(test_clock().Now(),
            prefs()->GetTime(kDomainDiversityReportingTimestamp));
}

TEST_F(DomainDiversityReporterTest, OnlyOneReportPerDay) {
  ASSERT_TRUE(LoadHistory());

  base::Time last_midnight = MidnightNDaysLater(test_clock().Now(), -1);

  prefs()->SetTime(kDomainDiversityReportingTimestamp, last_midnight);

  CreateDomainDiversityReporter();
  task_environment_.RunUntilIdle();

  histograms().ExpectUniqueSample("History.DomainCount1Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount1Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V3", 0, 1);

  history_service()->AddPage(GURL("http://www.google.com"), test_clock().Now(),
                             history::VisitSource::SOURCE_BROWSED);

  // Set the mock clock to 20:00 on the same day
  test_clock().SetTime(MidnightNDaysLater(test_clock().Now(), 0) +
                       base::Hours(20));

  // Fast-forward the scheduler's clock by another 24 hours in order to trigger
  // the next report
  FastForwardAndWait(kScheduleInterval);

  // No new report since one report is already generated on the same day.
  // This could happen when the last report occurred very early
  // on a day longer than 24 hours (e.g. the day on which daylight saving
  // time ends).
  histograms().ExpectUniqueSample("History.DomainCount1Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V2", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount1Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount7Day_V3", 0, 1);
  histograms().ExpectUniqueSample("History.DomainCount28Day_V3", 0, 1);
}
}  // namespace history
