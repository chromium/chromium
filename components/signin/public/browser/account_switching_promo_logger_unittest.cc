// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/account_switching_promo_logger.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/browser/account_switching_promo_metrics.h"
#include "components/signin/public/browser/account_switching_promo_prefs.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountSwitchingPromoLoggerTest : public testing::Test {
 public:
  AccountSwitchingPromoLoggerTest() {
    RegisterAccountSwitchingPromoProfilePrefs(pref_service_.registry());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  testing::StrictMock<feature_engagement::test::MockTracker> tracker_;
  const GaiaId primary_ = GaiaId("gaia_primary");
  const GaiaId target_ = GaiaId("gaia_target");
  base::HistogramTester histogram_tester_;
};

// Tests that `LogPromoTriggered()` records `kPromoTriggered` without
// recording an outcome.
TEST_F(AccountSwitchingPromoLoggerTest, LogPromoTriggered) {
  std::optional<AccountSwitchingPromoLogger> logger;
  logger.emplace(tracker_, pref_service_, primary_, target_);

  // Simulate presentation of the promo being triggered.
  logger->LogPromoTriggered();

  histogram_tester_.ExpectUniqueSample(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kPromoTriggered, 1);
  EXPECT_CALL(
      tracker_,
      NotifyEvent(feature_engagement::events::kAccountSwitchingPromoIgnored));
  // Simulate the promo being torn down without any user interaction.
  logger.reset();
}

// Tests that `LogAccepted()` records `kAccepted` and updates prefs, and that
// destroying the logger does not record `kIgnored` afterwards.
TEST_F(AccountSwitchingPromoLoggerTest, LogAccepted) {
  {
    AccountSwitchingPromoLogger logger(tracker_, pref_service_, primary_,
                                       target_);
    // Simulate presentation of the promo being triggered.
    logger.LogPromoTriggered();

    // Simulate the user accepting the promo and the promo being torn down.
    logger.LogAccepted();
  }

  histogram_tester_.ExpectBucketCount(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kPromoTriggered, 1);
  histogram_tester_.ExpectBucketCount(kAccountSwitchingPromoEventsHistogram,
                                      AccountSwitchingPromoEvent::kAccepted, 1);
  histogram_tester_.ExpectTotalCount(kAccountSwitchingPromoEventsHistogram, 2);
  // Simulate the user switching back to `primary_` within 24 hours.
  MaybeRecordAccountSwitchingRetention(pref_service_, primary_);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours", true, 1);
}

// Tests that `LogExplicitlyDismissed()` records `kExplicitlyDismissed`,
// notifies the tracker, and records the dismissal of the (`primary_`,
// `target_`) pair.
TEST_F(AccountSwitchingPromoLoggerTest, LogExplicitlyDismissed) {
  EXPECT_CALL(tracker_,
              NotifyEvent(feature_engagement::events::
                              kAccountSwitchingPromoExplicitlyDismissed))
      .Times(2);
  // Simulate the promo being triggered and explicitly dismissed twice.
  for (int i = 0; i < 2; ++i) {
    AccountSwitchingPromoLogger logger(tracker_, pref_service_, primary_,
                                       target_);
    logger.LogPromoTriggered();
    logger.LogExplicitlyDismissed();
  }

  histogram_tester_.ExpectBucketCount(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kPromoTriggered, 2);
  histogram_tester_.ExpectBucketCount(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kExplicitlyDismissed, 2);
  histogram_tester_.ExpectTotalCount(kAccountSwitchingPromoEventsHistogram, 4);
  // Two explicit dismissals suppress the pair.
  EXPECT_FALSE(CanShowAccountSwitchingPromo(pref_service_, primary_, target_));
}

// Tests that destroying the logger records `kIgnored` when no outcome was
// recorded.
TEST_F(AccountSwitchingPromoLoggerTest, DestructorRecordsIgnored) {
  std::optional<AccountSwitchingPromoLogger> logger;
  logger.emplace(tracker_, pref_service_, primary_, target_);
  // Simulate presentation of the promo being triggered.
  logger->LogPromoTriggered();
  EXPECT_CALL(
      tracker_,
      NotifyEvent(feature_engagement::events::kAccountSwitchingPromoIgnored));

  // Simulate the promo being torn down without any user interaction.
  logger.reset();

  histogram_tester_.ExpectBucketCount(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kPromoTriggered, 1);
  histogram_tester_.ExpectBucketCount(kAccountSwitchingPromoEventsHistogram,
                                      AccountSwitchingPromoEvent::kIgnored, 1);
  histogram_tester_.ExpectTotalCount(kAccountSwitchingPromoEventsHistogram, 2);
}

}  // namespace signin
