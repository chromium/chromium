// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

// Pref names duplicated from active_primary_accounts_metrics_recorder.cc.
constexpr char kActiveAccountsPrefName[] = "signin.active_accounts";
constexpr char kTimerPrefName[] = "signin.active_accounts_last_emitted";

class ActivePrimaryAccountsMetricsRecorderTest : public testing::Test {
 public:
  ActivePrimaryAccountsMetricsRecorderTest() {
    ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
        local_state_.registry());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
};

TEST_F(ActivePrimaryAccountsMetricsRecorderTest, RecordsOnFirstStartup) {
  base::HistogramTester histograms;

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  // Since the metrics have never been recorded before, they should immediately
  // get emitted, though it may be via a posted task. Let that run. Note that
  // RunUntilIdle() doesn't advance the mock time, and doesn't run any delayed
  // tasks.
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 1);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       DoesntRecordOnSubsequentStartupSoon) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  // Since *less* time than the emission interval has passed, the metrics should
  // *not* get recorded.
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 0);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 0);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsOnSubsequentStartupLater) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, but more than a day (i.e. the
  // emission interval) ago.
  local_state_.SetTime(kTimerPrefName,
                       base::Time::Now() - (base::Days(1) + base::Hours(1)));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  // Since *more* time than the emission interval has passed, the metrics
  // *should* get recorded.
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 1);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest, RecordsOncePerInterval) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  // Since *less* time than the emission interval has passed, the metrics should
  // *not* get recorded.
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 0);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 0);

  // Let some time pass, but not enough to hit the emission time.
  task_environment_.FastForwardBy(base::Hours(22));
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 0);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 0);

  // Now pass the emission time; the metrics should get recorded.
  task_environment_.FastForwardBy(base::Hours(2));
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 1);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 1);

  // After another day, they should get recorded again.
  task_environment_.FastForwardBy(base::Hours(24));
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 2);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 2);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsNumberOfDistinctAccounts) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last7Days", 0);
  histograms.ExpectTotalCount("Signin.NumberOfActiveAccounts.Last28Days", 0);

  // After some time, the user signs in.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow("first_gaia");
  // After some more time, the user switches accounts.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow("second_gaia");
  // After some more time, the user switches back to the first account.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow("first_gaia");

  // Finally, enough time passes to trigger a metrics emission. The two distinct
  // accounts should show up.
  task_environment_.FastForwardBy(base::Hours(24));
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                /*sample=*/2, /*expected_bucket_count=*/1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsNumberOfAccountsIn7And28Days) {
  // The metrics were previously recorded, and the next emission is (24-13)==11
  // hours away.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(13));
  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);

  tracker.MarkAccountAsActiveNow("first_gaia");
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow("second_gaia");
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow("third_gaia");
  task_environment_.FastForwardBy(base::Days(3));

  task_environment_.FastForwardBy(base::Hours(12));
  // "first_gaia" was now last used 5.5 days ago, "second_gaia" 4.5 days ago,
  // and "third_gaia" 3.5 days ago. The next metrics emission is 23 hours away.

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // "first_gaia" was now last used 6.5 days ago, "second_gaia" 5.5 days ago,
    // and "third_gaia" 4.5 days ago.

    // All 3 accounts were used within the last 7 days.
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                  /*sample=*/3, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                  /*sample=*/3, /*expected_bucket_count=*/1);
  }

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // "first_gaia" was now last used 7.5 days ago, "second_gaia" 6.5 days ago,
    // and "third_gaia" 5.5 days ago.

    // The last 2 accounts were used in the last 7 days. All 3 accounts are
    // still in the 28-day window.
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                  /*sample=*/2, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                  /*sample=*/3, /*expected_bucket_count=*/1);
  }

  task_environment_.FastForwardBy(base::Days(20));
  // "first_gaia" was now last used 27.5 days ago, "second_gaia" 26.5 days ago,
  // and "third_gaia" 25.5 days ago.

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // "first_gaia" was now last used 28.5 days ago, "second_gaia" 27.5 days
    // ago, and "third_gaia" 26.5 days ago.

    // No accounts were active in the last 7 days. 2 accounts are still in the
    // 28-day window.
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                  /*sample=*/0, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                  /*sample=*/2, /*expected_bucket_count=*/1);
  }

  tracker.MarkAccountAsActiveNow("second_gaia");
  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // "second_gaia" was now last used 1 day ago, and "third_gaia" 27.5 days
    // ago.

    // The second account was now active again in the last 7 days.
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                  /*sample=*/1, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                  /*sample=*/2, /*expected_bucket_count=*/1);
  }
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest, CleansUpExpiredEntries) {
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(13));
  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);

  tracker.MarkAccountAsActiveNow("first_gaia");
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow("second_gaia");
  task_environment_.FastForwardBy(base::Hours(12));

  // Sanity check: Now both accounts should be stored in the pref.
  ASSERT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 2u);

  task_environment_.FastForwardBy(base::Days(27));
  // Now "first_gaia" was last used just over 28 days ago, so it should've been
  // removed from the pref. "second_gaia" was used just under 28 days ago, so
  // should still be there.
  EXPECT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 1u);
}

}  // namespace

}  // namespace signin
