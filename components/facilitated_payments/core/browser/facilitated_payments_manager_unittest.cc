// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
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
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

class FacilitatedPaymentsManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    check_allowlist_attempt_count_ = 1;
    // The default result is `kUnknown`. This can be updated asynchronously to
    // simulate delay in receiving decision.
    allowlist_result_ = optimization_guide::OptimizationGuideDecision::kUnknown;
    // The default result is `kPixCodeNotFound`. This can be updated
    // asynchronously to simulate delay in PIX code loading.
    pix_code_detection_result_ =
        mojom::PixCodeDetectionResult::kPixCodeNotFound;
    optimization_guide_decider_ =
        std::make_unique<MockOptimizationGuideDecider>();
    driver_ = std::make_unique<MockFacilitatedPaymentsDriver>(nullptr);
    client_ = std::make_unique<FacilitatedPaymentsClient>();
    manager_ = std::make_unique<FacilitatedPaymentsManager>(
        driver_.get(), client_.get(), optimization_guide_decider_.get());
  }

  void TearDown() override {
    allowlist_decision_timer_.Stop();
    page_load_timer_.Stop();
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
    allowlist_decision_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&FacilitatedPaymentsManagerTest::SetAllowlistDecision,
                       base::Unretained(this), decision));
  }

  // Checks if allowlist decision (true or false) is made. If not,
  // advances time by `kOptimizationGuideDeciderWaitTime` and checks again,
  // `kMaxAttemptsForAllowlistCheck` times.
  void AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached() {
    while (allowlist_result_ ==
               optimization_guide::OptimizationGuideDecision::kUnknown &&
           check_allowlist_attempt_count_ <
               manager_->kMaxAttemptsForAllowlistCheck) {
      FastForwardBy(manager_->kOptimizationGuideDeciderWaitTime);
      ++check_allowlist_attempt_count_;
    }
  }

  // Advance to a point in time when PIX code detection should have been
  // triggered.
  void AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision() {
    // The PIX code detection is triggered at least `kPageLoadWaitTime` after
    // page load.
    base::TimeDelta time_to_trigger_pix_code_detection = std::max(
        base::Seconds(0), manager_->kPageLoadWaitTime -
                              (check_allowlist_attempt_count_ - 1) *
                                  manager_->kOptimizationGuideDeciderWaitTime);
    FastForwardBy(time_to_trigger_pix_code_detection);
  }

  void SetPixCodeDetectionResult(mojom::PixCodeDetectionResult result) {
    pix_code_detection_result_ = result;
  }

  // Sets PIX code detection `result` after `delay`.
  void SimulateDelayedPageLoadWithPixCodeDetectionResult(
      base::TimeDelta delay,
      mojom::PixCodeDetectionResult result) {
    page_load_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(
            &FacilitatedPaymentsManagerTest::SetPixCodeDetectionResult,
            base::Unretained(this), result));
  }

  // Checks if a PIX code is found. If not, advances time by
  // `kRetriggerPixCodeDetectionWaitTime` and checks again
  // `kMaxAttemptsForPixCodeDetection` times.
  void AdvanceTimeToPixCodeFoundResultReceivedOrMaxAttemptsReached() {
    while (pix_code_detection_result_ ==
               mojom::PixCodeDetectionResult::kPixCodeNotFound &&
           manager_->pix_code_detection_attempt_count_ <
               manager_->kMaxAttemptsForPixCodeDetection) {
      FastForwardBy(manager_->kRetriggerPixCodeDetectionWaitTime);
    }
  }

  // Returns the number of attempts made at PIX code detection based on the
  // `page_load_delay`.
  int GetPixCodeDetectionAttemptCount(base::TimeDelta page_load_delay) {
    // PIX code detection is triggered for the first time at least
    // `kPageLoadWaitTime` after page load.
    if (page_load_delay <= manager_->kPageLoadWaitTime) {
      return 1;
    }
    // PIX code detection is attempted every
    // `kRetriggerPixCodeDetectionWaitTime`, and the total attempts is capped at
    // `kMaxAttemptsForPixCodeDetection`.
    return std::min(
        (int)std::ceil((page_load_delay - manager_->kPageLoadWaitTime) /
                       manager_->kRetriggerPixCodeDetectionWaitTime) +
            1,
        manager_->kMaxAttemptsForPixCodeDetection);
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
    task_environment_.RunUntilIdle();
  }

 protected:
  optimization_guide::OptimizationGuideDecision allowlist_result_;
  mojom::PixCodeDetectionResult pix_code_detection_result_;
  std::unique_ptr<MockOptimizationGuideDecider> optimization_guide_decider_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<MockFacilitatedPaymentsDriver> driver_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<FacilitatedPaymentsManager> manager_;

 private:
  // Number of attempts at checking the allowlist.
  int check_allowlist_attempt_count_;
  base::OneShotTimer allowlist_decision_timer_;
  base::OneShotTimer page_load_timer_;
};

// Test that the `PIX_PAYMENT_MERCHANT_ALLOWLIST` optimization type is
// registered when RegisterPixOptimizationGuide is called.
TEST_F(FacilitatedPaymentsManagerTest, RegisterPixAllowlist) {
  EXPECT_CALL(*optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_PAYMENT_MERCHANT_ALLOWLIST)))
      .Times(1);

  manager_->RegisterPixAllowlist();
}

// Test that the PIX code detection is triggered for webpages in the allowlist.
TEST_F(FacilitatedPaymentsManagerTest,
       UrlInAllowlist_PixCodeDetectionTriggered) {
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

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the PIX code detection is not triggered for webpages not in the
// allowlist.
TEST_F(FacilitatedPaymentsManagerTest,
       UrlNotInAllowlist_PixCodeDetectionNotTriggered) {
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

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that if the allowlist checking infra is not ready after
// `kMaxAttemptsForAllowlistCheck` attempts, PIX code detection is not
// triggered.
TEST_F(FacilitatedPaymentsManagerTest,
       CheckAllowlistResultUnknown_PixCodeDetectionNotTriggered) {
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
      .Times(manager_->kMaxAttemptsForAllowlistCheck)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    CheckAllowlistResultShortDelay_UrlInAllowlist_PixCodeDetectionTriggered) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 1.6s and
  // returns positive decision.
  base::TimeDelta decision_delay = base::Seconds(1.6);
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
      .Times(std::ceil(decision_delay /
                       manager_->kOptimizationGuideDeciderWaitTime) +
             1)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(1);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    CheckAllowlistResultShortDelay_UrlNotInAllowlist_PixCodeDetectionNotTriggered) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 1.6s and
  // returns negative decision.
  base::TimeDelta decision_delay = base::Seconds(1.6);
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
      .Times(std::ceil(decision_delay /
                       manager_->kOptimizationGuideDeciderWaitTime) +
             1)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that the allowlist decision infra is given some time (short) to start-up
// and make decision. If the infra does not get ready within the given time,
// then PIX code detection is not run even if the infra eventually returns a
// decision.
TEST_F(
    FacilitatedPaymentsManagerTest,
    CheckAllowlistResultLongDelay_UrlInAllowlist_PixCodeDetectionNotTriggered) {
  GURL url("https://example.com/");

  // Simulate that the allowlist checking infra gets ready after 3.6s and
  // returns positive decision.
  base::TimeDelta decision_delay = base::Seconds(3.6);
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
      .Times(manager_->kMaxAttemptsForAllowlistCheck)
      .WillRepeatedly(testing::ReturnPointee(&allowlist_result_));
  EXPECT_CALL(*driver_, TriggerPixCodeDetection).Times(0);

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
}

// Test that if a PIX code does not exist on the page, multiple attempts are
// made to find PIX code, and finally `kPixCodeNotFound` is logged.
TEST_F(FacilitatedPaymentsManagerTest,
       NoPixCode_PixCodeNotFoundLoggedAfterMaxAttempts) {
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
  // Run the callback with the current result which can be updated
  // asynchronously. In this test, the result is not updated, so the result is
  // always the default `kPixCodeNotFound`.
  EXPECT_CALL(*driver_, TriggerPixCodeDetection)
      .Times(manager_->kMaxAttemptsForPixCodeDetection)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          testing::ByRef(pix_code_detection_result_)));

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
  AdvanceTimeToPixCodeFoundResultReceivedOrMaxAttemptsReached();

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since the PIX code does not exist on the page,
  // `kPixCodeNotFound` is logged after max attempts.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(mojom::PixCodeDetectionResult::kPixCodeNotFound));
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"),
            manager_->kMaxAttemptsForPixCodeDetection);
}

// Test UKM logging when the result of PIX code detection is received. This test
// is for the case when PIX code was not found.
TEST_F(FacilitatedPaymentsManagerTest, NoPixCode_NoUkm) {
  // To set the attempts and start the latency measuring timer. This call
  // actually doesn't trigger PIX code detection.
  manager_->TriggerPixCodeDetection();
  FastForwardBy(base::Milliseconds(200));
  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kPixCodeNotFound);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kLatencyInMillisName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since there is no PIX code, no is UKM logged as PIX code
  // detection gets re-triggered.
  EXPECT_EQ(ukm_entries.size(), 0UL);
}

// When the renderer returns the result of the document scan for PIX codes, a
// result of `kPixCodeNotFound` is treated differently when compared to other
// possible results. This class helps test those other possible results in a
// parametrized test.
class FacilitatedPaymentsManagerTestWhenPixCodeExists
    : public FacilitatedPaymentsManagerTest,
      public ::testing::WithParamInterface<mojom::PixCodeDetectionResult> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    FacilitatedPaymentsManagerTestWhenPixCodeExists,
    testing::Values(mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun,
                    mojom::PixCodeDetectionResult::kInvalidPixCodeFound,
                    mojom::PixCodeDetectionResult::kValidPixCodeFound));

// Test that if the page contents (specifically PIX code) have already loaded
// when PIX code detection is run, the result is logged immediately.
TEST_P(FacilitatedPaymentsManagerTestWhenPixCodeExists,
       PageAlreadyLoaded_ResultLoggedInSingleAttempt) {
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
  // Run the callback with different results.
  EXPECT_CALL(*driver_, TriggerPixCodeDetection)
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<0>(GetParam()));

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since the page contents (specifically PIX code) had already
  // loaded when PIX code detection was run, they are logged in the first
  // attempt.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(GetParam()));
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"), 1);
}

// Test that we allow a short duration (`kPageLoadWaitTime`) for page contents
// (specifically PIX code) to load after the `WebContentsObserver` informs about
// the page load event. If the contents load within this time, the result is
// logged in the first attempt.
TEST_P(FacilitatedPaymentsManagerTestWhenPixCodeExists,
       ShortPageLoadDelay_ResultLoggedInSingleAttempt) {
  GURL url("https://example.com/");
  SetAllowlistDecision(optimization_guide::OptimizationGuideDecision::kTrue);

  // Simulate that the page contents take a short time (1.6s) to finish loading.
  base::TimeDelta page_load_delay = base::Seconds(1.6);
  SimulateDelayedPageLoadWithPixCodeDetectionResult(page_load_delay,
                                                    GetParam());

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
  // Run the callback with the current result which can be updated
  // asynchronously.
  EXPECT_CALL(*driver_, TriggerPixCodeDetection)
      .Times(1)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          testing::ByRef(pix_code_detection_result_)));

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
  AdvanceTimeToPixCodeFoundResultReceivedOrMaxAttemptsReached();

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since the page contents (specifically PIX code) finished
  // loading soon (within `kPageLoadWaitTime`), the result is logged in the
  // first attempt.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(GetParam()));
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"), 1);
}

// Test that if the page contents do not load within `kPageLoadWaitTime`, then
// we retry PIX code detection. If the page contents finish loading within a
// reasonable time frame, the result is logged after a few attempts.
TEST_P(FacilitatedPaymentsManagerTestWhenPixCodeExists,
       MediumPageLoadDelay_ResultLoggedAfterMultipleAttempts) {
  GURL url("https://example.com/");
  SetAllowlistDecision(optimization_guide::OptimizationGuideDecision::kTrue);

  // Simulate that the page contents take a slightly longer time (3.6s) to
  // finish loading.
  base::TimeDelta page_load_delay = base::Seconds(3.6);
  SimulateDelayedPageLoadWithPixCodeDetectionResult(page_load_delay,
                                                    GetParam());

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
  // Run the callback with the current result which can be updated
  // asynchronously.
  EXPECT_CALL(*driver_, TriggerPixCodeDetection)
      .Times(GetPixCodeDetectionAttemptCount(page_load_delay))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          testing::ByRef(pix_code_detection_result_)));

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
  AdvanceTimeToPixCodeFoundResultReceivedOrMaxAttemptsReached();

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since the page contents (specifically PIX code) did not finish
  // loading within `kPageLoadWaitTime`, but did finish shortly after, the
  // result is logged after a few attempts.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(GetParam()));
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"),
            GetPixCodeDetectionAttemptCount(page_load_delay));
}

// Test that if the page contents take a long time to load, and have not loaded
// after repeated attempts at PIX code detection, `kPixCodeNotFound` is logged.
TEST_P(FacilitatedPaymentsManagerTestWhenPixCodeExists,
       LongPageLoadDelay_PixCodeNotFoundLoggedAfterMaxAttempts) {
  GURL url("https://example.com/");
  SetAllowlistDecision(optimization_guide::OptimizationGuideDecision::kTrue);

  // Simulate that the page contents take a long time (10.6s) to finish loading.
  base::TimeDelta page_load_delay = base::Seconds(10.6);
  SimulateDelayedPageLoadWithPixCodeDetectionResult(page_load_delay,
                                                    GetParam());

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
  // Run the callback with the current result which can be updated
  // asynchronously.
  EXPECT_CALL(*driver_, TriggerPixCodeDetection)
      .Times(manager_->kMaxAttemptsForPixCodeDetection)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          testing::ByRef(pix_code_detection_result_)));

  manager_->DelayedCheckAllowlistAndTriggerPixCodeDetection(
      url, ukm::UkmRecorder::GetNewSourceID());
  AdvanceTimeToAllowlistDecisionReceivedOrMaxAttemptsReached();
  AdvanceTimeToPotentiallyTriggerPixCodeDetectionAfterDecision();
  AdvanceTimeToPixCodeFoundResultReceivedOrMaxAttemptsReached();

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that since the page contents (specifically PIX code) took too long
  // to load, `kPixCodeNotFound` is logged after max attempts.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(mojom::PixCodeDetectionResult::kPixCodeNotFound));
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"),
            manager_->kMaxAttemptsForPixCodeDetection);
}

// Test UKM logging when the result of PIX code detection is received.
TEST_P(FacilitatedPaymentsManagerTestWhenPixCodeExists, Ukm) {
  // To set the attempts and start the latency measuring timer. This call
  // actually doesn't trigger PIX code detection.
  manager_->TriggerPixCodeDetection();
  FastForwardBy(base::Milliseconds(200));
  manager_->ProcessPixCodeDetectionResult(GetParam());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kResultName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kLatencyInMillisName,
       ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kAttemptsName});

  // Verify that the UKM metrics are logged.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(GetParam()));
  // Verify that the simulated latency is logged and is within a small time
  // margin accounting for computation.
  EXPECT_GE(ukm_entries[0].metrics.at("LatencyInMillis"), 200);
  EXPECT_NEAR(ukm_entries[0].metrics.at("LatencyInMillis"), 200, 5);
  EXPECT_EQ(ukm_entries[0].metrics.at("Attempts"), 1);
}

}  // namespace payments::facilitated
