// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class MockFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  explicit MockFacilitatedPaymentsDriver(
      std::unique_ptr<FacilitatedPaymentsManager> manager)
      : FacilitatedPaymentsDriver(std::move(manager)) {}
  ~MockFacilitatedPaymentsDriver() override = default;

  MOCK_METHOD(void,
              TriggerPixCodeDetection,
              (base::OnceCallback<void(mojom::PixCodeDetectionResult)>),
              (override));
};

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       optimization_guide::proto::RequestContextMetadata*
           request_context_metadata),
      (override));
};

class FacilitatedPaymentsManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    attempt_number_ = 1;
    allowlist_result_ = optimization_guide::OptimizationGuideDecision::kUnknown;
    timer_.Stop();
    optimization_guide_decider_ =
        std::make_unique<MockOptimizationGuideDecider>();
    driver_ = std::make_unique<MockFacilitatedPaymentsDriver>(nullptr);
    manager_ = std::make_unique<FacilitatedPaymentsManager>(
        driver_.get(), optimization_guide_decider_.get(),
        ukm::UkmRecorder::GetNewSourceID());
  }

  optimization_guide::OptimizationGuideDecision& GetAllowlistCheckResult() {
    return allowlist_result_;
  }

  // Sets the allowlist `decision` (true or false).
  void SetAllowlistDecision(
      optimization_guide::OptimizationGuideDecision decision) {
    allowlist_result_ = decision;
  }

  // Sets allowlist `decision` after `delay`.
  void SimulateDelayedAllowlistDecision(
      base::TimeDelta delay,
      optimization_guide::OptimizationGuideDecision decision) {
    timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&FacilitatedPaymentsManagerTest::SetAllowlistDecision,
                       base::Unretained(this), decision));
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
    task_environment_.RunUntilIdle();
  }

  // Checks if allowlist decision (true or false) is made. If not,
  // advances time by `kOptimizationGuideDeciderWaitTime` and checks again,
  // `kMaxAttemptsForAllowlistCheck` times.
  void AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached() {
    while (allowlist_result_ ==
               optimization_guide::OptimizationGuideDecision::kUnknown &&
           attempt_number_ < kMaxAttemptsForAllowlistCheck) {
      FastForwardBy(kOptimizationGuideDeciderWaitTime);
      ++attempt_number_;
    }
  }

  // Advance to a point in time when PIX code detection should have been
  // triggered.
  void AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision() {
    // The PIX code detection is triggered at least `kPageLoadWaitTime` after
    // page load.
    base::TimeDelta time_to_trigger_pix_detection =
        std::max(base::Seconds(0),
                 kPageLoadWaitTime -
                     (attempt_number_ - 1) * kOptimizationGuideDeciderWaitTime);
    FastForwardBy(time_to_trigger_pix_detection);
  }

 protected:
  optimization_guide::OptimizationGuideDecision allowlist_result_;
  std::unique_ptr<MockOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<MockFacilitatedPaymentsDriver> driver_;
  std::unique_ptr<FacilitatedPaymentsManager> manager_;

 private:
  int attempt_number_;  // Number of attempts at checking the allowlist.
  base::OneShotTimer timer_;
};

// Test that the `PIX_PAYMENT_MERCHANT_ALLOWLIST` optimization type is
// registered when RegisterPixOptimizationGuide is called.
TEST_F(FacilitatedPaymentsManagerTest, TestRegisterPixAllowlist) {
  EXPECT_CALL(*optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST)))
      .Times(1);

  manager_->RegisterPixAllowlist();
}

// Test that the PIX code detection is triggered for webpages in the allowlist.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_InAllowlistDecision) {
  GURL url("https://example.com/");
  SetAllowlistDecision(optimization_guide::OptimizationGuideDecision::kTrue);

  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(1);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the PIX code detection is not triggered for webpages not in the
// allowlist.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_NotInAllowlistDecision) {
  GURL url("https://example.com/");
  SetAllowlistDecision(optimization_guide::OptimizationGuideDecision::kFalse);

  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that if the allowlist checking infra is not ready after
// `kMaxAttemptsForAllowlistCheck` attempts, PIX code detection is not
// triggered.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_DecisionDelay_NoDecision) {
  GURL url("https://example.com/");

  // The default decision is kUnknown.
  // Allowlist check should be attempted once every
  // `kOptimizationGuideDeciderWaitTime` until decision is received or
  // `kMaxAttemptsForAllowlistCheck` attempts are made.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(kMaxAttemptsForAllowlistCheck)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_DecisionDelay_InAllowlistDecision) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 1.5s and
  // returns positive decision.
  base::TimeDelta decision_delay = base::Seconds(1.5);
  SimulateDelayedAllowlistDecision(
      decision_delay, optimization_guide::OptimizationGuideDecision::kTrue);

  // Allowlist check should be attempted once every
  // `kOptimizationGuideDeciderWaitTime` until decision is received.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(decision_delay / kOptimizationGuideDeciderWaitTime + 1)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(1);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_DecisionDelay_NotInAllowlistDecision) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 1.5s and
  // returns negative decision.
  base::TimeDelta decision_delay = base::Seconds(1.5);
  SimulateDelayedAllowlistDecision(
      decision_delay, optimization_guide::OptimizationGuideDecision::kFalse);

  // Allowlist check should be attempted once every
  // `kOptimizationGuideDeciderWaitTime` until decision is received.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(decision_delay / kOptimizationGuideDeciderWaitTime + 1)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision. If the infra does not get ready within the given time,
// then PIX code detection is not run even if the infra eventually returns a
// decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    TestDelayedCheckAllowlistAndTriggerPixCodeDetection_DecisionDelay_LongDelay_InAllowlistDecision) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 3.5s and
  // returns positive decision.
  base::TimeDelta decision_delay = base::Seconds(3.5);
  SimulateDelayedAllowlistDecision(
      decision_delay, optimization_guide::OptimizationGuideDecision::kTrue);

  // The default decision is kUnknown.
  // Allowlist check should be attempted once every
  // `kOptimizationGuideDeciderWaitTime` until decision is received or
  // `kMaxAttemptsForAllowlistCheck` attempts are made.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(kMaxAttemptsForAllowlistCheck)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(url);
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

class FacilitatedPaymentsManagerMetricsTest
    : public FacilitatedPaymentsManagerTest,
      public ::testing::WithParamInterface<mojom::PixCodeDetectionResult> {
 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FacilitatedPaymentsManagerMetricsTest,
    testing::Values(mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun,
                    mojom::PixCodeDetectionResult::kPixCodeNotFound,
                    mojom::PixCodeDetectionResult::kInvalidPixCodeFound,
                    mojom::PixCodeDetectionResult::kValidPixCodeFound));

// Test that UKM metrics are recorded.
TEST_P(FacilitatedPaymentsManagerMetricsTest,
       TestProcessPixCodeDetectionResult_VerifyResultAndLatencyUkmLogged) {
  // Start the latency measuring timer, and advance 200ms to the future.
  manager_->StartPixCodeDetectionLatencyTimer();
  FastForwardBy(base::Milliseconds(200));
  manager_->ProcessPixCodeDetectionResult(GetParam());

  // Verify that the result passed are logged.
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kLatencyInMillisName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(GetParam()));
  // Verify that the simulated latency is logged and is within a small time
  // margin accounting for computation.
  EXPECT_GE(ukm_entries[0].metrics.at("LatencyInMillis"), 200);
  EXPECT_NEAR(ukm_entries[0].metrics.at("LatencyInMillis"), 200, 5);
}

}  // namespace payments::facilitated
