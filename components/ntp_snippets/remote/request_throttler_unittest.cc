// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/request_throttler.h"

#include <memory>

#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kCounterQuota = 2;
}  // namespace

namespace ntp_snippets {

class RequestThrottlerTest : public testing::Test {
 public:
  RequestThrottlerTest() {
    RequestThrottler::RegisterProfilePrefs(test_prefs_.registry());
    // Use any arbitrary RequestType for this unittest.
    throttler_ = std::make_unique<RequestThrottler>(
        &test_prefs_, RequestThrottler::RequestType::
                          CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER);
    throttler_->quota_ = kCounterQuota;
  }
  RequestThrottlerTest(const RequestThrottlerTest&) = delete;
  RequestThrottlerTest& operator=(const RequestThrottlerTest&) = delete;

 protected:
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<RequestThrottler> throttler_;
};

TEST_F(RequestThrottlerTest, QuotaExceeded) {
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_FALSE(throttler_->DemandQuotaForRequest(false));
}

TEST_F(RequestThrottlerTest, ForcedDoesNotCountInQuota) {
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(true));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
}

TEST_F(RequestThrottlerTest, ForcedWorksRegardlessOfQuota) {
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(true));
}

TEST_F(RequestThrottlerTest, QuotaIsPerDay) {
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));

  // Now fake the day pref so that the counter believes the count comes from
  // yesterday.
  int now_day = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  test_prefs_.SetInteger(ntp_snippets::prefs::kSnippetFetcherRequestsDay,
                         now_day - 1);

  // The quota should get reset as the day has changed.
  EXPECT_TRUE(throttler_->DemandQuotaForRequest(false));
}

}  // namespace ntp_snippets
