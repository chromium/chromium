// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/refresh_throttler.h"

#include <limits>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/user_classifier.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

}  // namespace

class RefreshThrottlerTest : public testing::Test {
 public:
  RefreshThrottlerTest() {
    feed::RegisterProfilePrefs(test_prefs_.registry());

    base::Time now;
    EXPECT_TRUE(base::Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kInterestFeedContentSuggestions,
        {{"quota_SuggestionFetcherActiveNTPUser", "2"}});

    throttler_ = std::make_unique<RefreshThrottler>(
        UserClassifier::UserClass::kActiveSuggestionsViewer, &test_prefs_,
        &test_clock_);
  }

 protected:
  TestingPrefServiceSimple test_prefs_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<RefreshThrottler> throttler_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RefreshThrottlerTest);
};

TEST_F(RefreshThrottlerTest, QuotaExceeded) {
  EXPECT_TRUE(throttler_->RequestQuota());
  EXPECT_TRUE(throttler_->RequestQuota());
  EXPECT_FALSE(throttler_->RequestQuota());
}

TEST_F(RefreshThrottlerTest, QuotaIsPerDay) {
  EXPECT_TRUE(throttler_->RequestQuota());
  EXPECT_TRUE(throttler_->RequestQuota());
  EXPECT_FALSE(throttler_->RequestQuota());

  test_clock_.Advance(base::TimeDelta::FromDays(1));
  EXPECT_TRUE(throttler_->RequestQuota());
}

TEST_F(RefreshThrottlerTest, RollOver) {
  // Exhaust our quota so the for loop can verify everything as false.
  EXPECT_TRUE(throttler_->RequestQuota());
  EXPECT_TRUE(throttler_->RequestQuota());

  test_clock_.SetNow(test_clock_.Now().LocalMidnight());
  for (int i = 0; i < 24; i++) {
    EXPECT_FALSE(throttler_->RequestQuota());
    test_clock_.Advance(base::TimeDelta::FromHours(1));
  }
  EXPECT_TRUE(throttler_->RequestQuota());
}

TEST_F(RefreshThrottlerTest, Overflow) {
  test_prefs_.SetInteger(prefs::kThrottlerRequestCount,
                         std::numeric_limits<int>::max());
  EXPECT_FALSE(throttler_->RequestQuota());
  EXPECT_EQ(std::numeric_limits<int>::max(),
            test_prefs_.GetInteger(prefs::kThrottlerRequestCount));
}

}  // namespace feed
