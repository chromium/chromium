// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"

#include <array>
#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

// Pref names duplicated from active_primary_accounts_metrics_recorder.cc.
constexpr char kActiveAccountsPrefName[] = "signin.active_accounts";
constexpr char kActiveAccountsManagedPrefName[] =
    "signin.active_accounts_managed";
constexpr char kTimerPrefName[] = "signin.active_accounts_last_emitted";
#if BUILDFLAG(IS_IOS)
constexpr char kAccountSwitchTimestampsPrefName[] =
    "signin.account_switch_timestamps";
#endif  // BUILDFLAG(IS_IOS)

constexpr std::array<std::string_view, 4>
    kUnconditionalNumberOfActiveAccountsHistograms = {
        "Signin.NumberOfActiveAccounts.Last7Days",
        "Signin.NumberOfActiveAccounts.Last28Days",
        "Signin.NumberOfActiveManagedAccounts.Last7Days",
        "Signin.NumberOfActiveManagedAccounts.Last28Days"};

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
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 1);
  }
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
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }
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
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 1);
  }
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest, RecordsOncePerInterval) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  // Since *less* time than the emission interval has passed, the metrics should
  // *not* get recorded.
  task_environment_.RunUntilIdle();
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }

  // Let some time pass, but not enough to hit the emission time.
  task_environment_.FastForwardBy(base::Hours(22));
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }

  // Now pass the emission time; the metrics should get recorded.
  task_environment_.FastForwardBy(base::Hours(2));
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 1);
  }

  // After another day, they should get recorded again.
  task_environment_.FastForwardBy(base::Hours(24));
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 2);
  }
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsNumberOfDistinctAccounts) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  task_environment_.RunUntilIdle();
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days", 0);
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days", 0);

  // After some time, the user signs in.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);
  // After some more time, the user switches accounts.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow(GaiaId("second_gaia"), Tribool::kFalse);
  // After some more time, the user switches back to the first account.
  task_environment_.FastForwardBy(base::Hours(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);

  // Finally, enough time passes to trigger a metrics emission. The two distinct
  // accounts should show up.
  task_environment_.FastForwardBy(base::Hours(24));
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                /*sample=*/2, /*expected_bucket_count=*/1);
  // None of the accounts were managed.
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days",
      /*expected_count=*/0);
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days",
      /*expected_count=*/0);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last7Days",
      /*sample=*/0, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last28Days",
      /*sample=*/0, /*expected_bucket_count=*/1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest, RecordsManagednessOfAccounts) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  task_environment_.RunUntilIdle();
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days", 0);
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days", 0);

  // A personal account signs in.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);
  // After some more time, the user switches to a managed account.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_managed_gaia"), Tribool::kTrue);
  // After some more time, the user switches to another managed account.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("second_managed_gaia"), Tribool::kTrue);

  // Enough time passes to trigger a metrics emission.
  task_environment_.FastForwardBy(base::Hours(24));

  // There should be 3 accounts total.
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                /*sample=*/3, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                /*sample=*/3, /*expected_bucket_count=*/1);

  // Some of the accounts were managed, so "AnyManaged" should be recorded.
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days",
      /*sample=*/3, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days",
      /*sample=*/3, /*expected_bucket_count=*/1);

  // And finally, there should be 2 managed accounts.
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last7Days",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last28Days",
      /*sample=*/2, /*expected_bucket_count=*/1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsManagednessOfAccountsWhenDeterminedLater) {
  base::HistogramTester histograms;

  // The metrics were previously recorded, not too long ago.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(1));

  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);
  task_environment_.RunUntilIdle();
  for (std::string_view histogram_name :
       kUnconditionalNumberOfActiveAccountsHistograms) {
    histograms.ExpectTotalCount(histogram_name, 0);
  }
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days", 0);
  histograms.ExpectTotalCount(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days", 0);

  // A personal account signs in.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);
  // After some more time, the user switches to a managed account. The
  // managed-ness is initially unknown, but gets determined soon.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("first_managed_gaia"),
                                 Tribool::kUnknown);
  task_environment_.FastForwardBy(base::Seconds(5));
  tracker.MarkAccountAsManaged(GaiaId("first_managed_gaia"), true);
  // After some more time, the user switches to another managed account, but the
  // managed-ness is not known yet.
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.MarkAccountAsActiveNow(GaiaId("second_managed_gaia"),
                                 Tribool::kUnknown);

  // There are 3 accounts now, but only 2 of them have a known managed-ness.
  ASSERT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 3u);
  EXPECT_EQ(local_state_.GetDict(kActiveAccountsManagedPrefName).size(), 2u);

  // Enough time passes to trigger a metrics emission.
  task_environment_.FastForwardBy(base::Hours(24));

  // There should be 3 accounts total.
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last7Days",
                                /*sample=*/3, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Signin.NumberOfActiveAccounts.Last28Days",
                                /*sample=*/3, /*expected_bucket_count=*/1);

  // One of the accounts was (known to be) managed, so "AnyManaged" should be
  // recorded.
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last7Days",
      /*sample=*/3, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveAccounts.AnyManaged.Last28Days",
      /*sample=*/3, /*expected_bucket_count=*/1);

  // Only one account should be recorded as managed - "unknown" defaults to "not
  // managed" for these metrics.
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last7Days",
      /*sample=*/1, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      "Signin.NumberOfActiveManagedAccounts.Last28Days",
      /*sample=*/1, /*expected_bucket_count=*/1);

  // Finally, the managed-ness of the second account gets determined.
  tracker.MarkAccountAsManaged(GaiaId("second_managed_gaia"), true);

  ASSERT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 3u);
  EXPECT_EQ(local_state_.GetDict(kActiveAccountsManagedPrefName).size(), 3u);

  // The next metrics emission should recognize both managed accounts.
  task_environment_.FastForwardBy(base::Hours(24));
  histograms.ExpectBucketCount("Signin.NumberOfActiveManagedAccounts.Last7Days",
                               /*sample=*/2, /*expected_count=*/1);
  histograms.ExpectBucketCount(
      "Signin.NumberOfActiveManagedAccounts.Last28Days",
      /*sample=*/2, /*expected_count=*/1);
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsNumberOfAccountsIn7And28Days) {
  // The metrics were previously recorded, and the next emission is (24-13)==11
  // hours away.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(13));
  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);

  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow(GaiaId("second_gaia"), Tribool::kFalse);
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow(GaiaId("third_gaia"), Tribool::kFalse);
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

  tracker.MarkAccountAsActiveNow(GaiaId("second_gaia"), Tribool::kFalse);
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

  tracker.MarkAccountAsActiveNow(GaiaId("first_gaia"), Tribool::kFalse);
  task_environment_.FastForwardBy(base::Days(1));
  tracker.MarkAccountAsActiveNow(GaiaId("second_gaia"), Tribool::kFalse);
  task_environment_.FastForwardBy(base::Hours(12));

  // Sanity check: Now both accounts should be stored in the pref.
  ASSERT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 2u);
  ASSERT_EQ(local_state_.GetDict(kActiveAccountsManagedPrefName).size(), 2u);

  task_environment_.FastForwardBy(base::Days(27));
  // Now "first_gaia" was last used just over 28 days ago, so it should've been
  // removed from the pref. "second_gaia" was used just under 28 days ago, so
  // should still be there.
  EXPECT_EQ(local_state_.GetDict(kActiveAccountsPrefName).size(), 1u);
  EXPECT_EQ(local_state_.GetDict(kActiveAccountsManagedPrefName).size(), 1u);
}

#if BUILDFLAG(IS_IOS)

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       RecordsNumberOfAccountSwitches) {
  // The metrics were previously recorded, and the next emission is (24-13)==11
  // hours away.
  local_state_.SetTime(kTimerPrefName, base::Time::Now() - base::Hours(13));
  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);

  task_environment_.FastForwardBy(base::Days(1));
  tracker.AccountWasSwitched();
  task_environment_.FastForwardBy(base::Days(1));
  tracker.AccountWasSwitched();
  task_environment_.FastForwardBy(base::Days(4));

  ASSERT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(), 2u);

  task_environment_.FastForwardBy(base::Hours(12));
  // The user switched from "first_gaia" to "second_gaia" 5.5 days ago, and to
  // "third_gaia" 4.5 days ago. The next metrics emission is 23 hours away.

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // The first account switch is now 6.5 days ago, and the second 5.5 days
    // ago.
    histograms.ExpectUniqueSample("Signin.IOSNumberOfAccountSwitches.Last7Days",
                                  /*sample=*/2, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample(
        "Signin.IOSNumberOfAccountSwitches.Last28Days",
        /*sample=*/2, /*expected_bucket_count=*/1);

    EXPECT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
              2u);
  }

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // The first account switch is now 7.5 days ago, and the second 6.5 days
    // ago.
    histograms.ExpectUniqueSample("Signin.IOSNumberOfAccountSwitches.Last7Days",
                                  /*sample=*/1, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample(
        "Signin.IOSNumberOfAccountSwitches.Last28Days",
        /*sample=*/2, /*expected_bucket_count=*/1);

    EXPECT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
              2u);
  }

  task_environment_.FastForwardBy(base::Days(20));
  // The first account switch is now 27.5 days ago, and the second 26.5 days
  // ago.

  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // The first account switch is now 28.5 days ago, and the second 27.5 days
    // ago.
    histograms.ExpectUniqueSample("Signin.IOSNumberOfAccountSwitches.Last7Days",
                                  /*sample=*/0, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample(
        "Signin.IOSNumberOfAccountSwitches.Last28Days",
        /*sample=*/1, /*expected_bucket_count=*/1);

    EXPECT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
              1u);
  }

  tracker.AccountWasSwitched();
  {
    base::HistogramTester histograms;

    task_environment_.FastForwardBy(base::Days(1));
    // The original two account switches are out of the 28-day window now, and
    // only the one recent switch should be recorded.
    histograms.ExpectUniqueSample("Signin.IOSNumberOfAccountSwitches.Last7Days",
                                  /*sample=*/1, /*expected_bucket_count=*/1);
    histograms.ExpectUniqueSample(
        "Signin.IOSNumberOfAccountSwitches.Last28Days",
        /*sample=*/1, /*expected_bucket_count=*/1);

    EXPECT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
              1u);
  }
}

TEST_F(ActivePrimaryAccountsMetricsRecorderTest,
       LimitsTrackedNumberOfSwitches) {
  ActivePrimaryAccountsMetricsRecorder tracker(local_state_);

  // A reasonable number of switches should be tracked accurately in the pref.
  constexpr size_t kSmallNumberOfSwitches = 10;
  for (size_t i = 0; i < kSmallNumberOfSwitches; i++) {
    task_environment_.FastForwardBy(base::Seconds(1));
    tracker.AccountWasSwitched();
  }
  EXPECT_EQ(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
            kSmallNumberOfSwitches);

  // A very large number of switches should not be tracked in the pref anymore
  // - there should be a limit.
  constexpr size_t kLargeNumberOfSwitches = 200;
  for (size_t i = 0; i < kLargeNumberOfSwitches; i++) {
    task_environment_.FastForwardBy(base::Seconds(1));
    tracker.AccountWasSwitched();
  }
  EXPECT_GT(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
            kSmallNumberOfSwitches);
  EXPECT_LT(local_state_.GetList(kAccountSwitchTimestampsPrefName).size(),
            kLargeNumberOfSwitches);
}

#endif  // BUILDFLAG(IS_IOS)

}  // namespace

}  // namespace signin
