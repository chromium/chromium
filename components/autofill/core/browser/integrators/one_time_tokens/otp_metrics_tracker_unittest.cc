// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::NiceMock;

class OtpMetricsTrackerTest : public testing::Test {
 public:
  OtpMetricsTrackerTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service_;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      subscription_manager_;
};

TEST_F(OtpMetricsTrackerTest, NullServiceDoesNotCrash) {
  OtpMetricsTracker tracker(/*one_time_token_service=*/nullptr);
  EXPECT_FALSE(tracker.HasActiveSubscriptionForTesting());
}

TEST_F(OtpMetricsTrackerTest, SubscribesUponConstruction) {
  EXPECT_CALL(mock_ott_service_,
              SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail,
                                 base::Time::Max(), _))
      .WillOnce(
          [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                 one_time_tokens::OneTimeTokenService::TickleCallback cb) {
            return subscription_manager_.Subscribe(
                exp, std::move(cb), /*expiration_callback=*/base::DoNothing());
          });

  OtpMetricsTracker tracker(&mock_ott_service_);
  EXPECT_TRUE(tracker.HasActiveSubscriptionForTesting());
}

}  // namespace
}  // namespace autofill
