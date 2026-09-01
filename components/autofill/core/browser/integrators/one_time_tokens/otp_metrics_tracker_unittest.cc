// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
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

  void SetUp() override {
    ON_CALL(mock_ott_service_,
            SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail,
                               base::Time::Max(), _))
        .WillByDefault(
            [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                   one_time_tokens::OneTimeTokenService::TickleCallback cb) {
              return subscription_manager_.Subscribe(
                  exp, std::move(cb),
                  /*expiration_callback=*/base::DoNothing());
            });
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kAutofillGmailOtp};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service_;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      subscription_manager_;
  base::HistogramTester histogram_tester_;
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

TEST_F(
    OtpMetricsTrackerTest,
    FieldDetectionToTickleLatency_LoggedWhenTickleArrivesAfterFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(500));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(500), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NotLoggedIfNoFieldDetected) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_OnlyFirstTickleLogged) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(200));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(200), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_LastFieldDetectionTimestampUsed) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(100));
  // Subsequent field detections update the timestamp to the last seen field.
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(200));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(200), 1);
}

TEST_F(
    OtpMetricsTrackerTest,
    FieldDetectionToTickleLatency_NotLoggedIfMoreThanFieldDetectionTimeoutPass) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(OtpMetricsTracker::kFieldDetectionTimeout +
                                  base::Milliseconds(1));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NewSessionAfterTickle) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // First session.
  tracker.OnOtpFieldDetected();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  // Second session.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  tracker.OnOtpFieldDetected();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 2);
  histogram_tester_.ExpectTimeBucketCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(250), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NotLoggedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(500));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_LoggedWhenFieldDetectedAfterTickle) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(300), 1);
  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(
    OtpMetricsTrackerTest,
    TickleToFieldDetectionLatency_NotLoggedIfMoreThanFieldDetectionTimeoutPass) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(OtpMetricsTracker::kFieldDetectionTimeout +
                                  base::Milliseconds(1));
  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_LastTickleTimestampUsed) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // First tickle.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Second tickle: updates tickle timestamp.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(250));

  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(250), 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_OnlyFirstFieldDetectionLogged) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(150));

  // First field detection: records latency and resets tickle timestamp.
  tracker.OnOtpFieldDetected();

  task_environment_.FastForwardBy(base::Milliseconds(200));
  // Subsequent field detection: should not record again.
  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(150), 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_NotLoggedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest, FieldDetectionAndTickle_BidirectionalSessions) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // Session 1: Tickle arrives first, then field detected.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(150));
  tracker.OnOtpFieldDetected();

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(150), 1);

  // Session 2: Field detected first, then tickle arrives.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  tracker.OnOtpFieldDetected();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(250), 1);
}

}  // namespace
}  // namespace autofill
