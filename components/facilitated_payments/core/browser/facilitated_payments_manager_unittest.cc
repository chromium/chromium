// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
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
              (base::OnceCallback<void(mojom::PixCodeDetectionResult,
                                       const std::string&)>),
              (override));
};

// A mock for the facilitated payment API client interface.
class MockFacilitatedPaymentsApiClient : public FacilitatedPaymentsApiClient {
 public:
  MockFacilitatedPaymentsApiClient() = default;
  ~MockFacilitatedPaymentsApiClient() override = default;

  MOCK_METHOD(void, IsAvailable, (base::OnceCallback<void(bool)>), (override));
  MOCK_METHOD(void,
              GetClientToken,
              (base::OnceCallback<void(std::vector<uint8_t>)>),
              (override));
  MOCK_METHOD(void,
              InvokePurchaseAction,
              (CoreAccountInfo,
               base::span<const uint8_t>,
               base::OnceCallback<void(PurchaseActionResult)>),
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

// A mock for the facilitated payment "client" interface.
class MockFacilitatedPaymentsClient : public FacilitatedPaymentsClient {
 public:
  MockFacilitatedPaymentsClient() = default;
  ~MockFacilitatedPaymentsClient() override = default;

  MOCK_METHOD(void,
              LoadRiskData,
              (base::OnceCallback<void(const std::string&)>),
              (override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (override));
  MOCK_METHOD(FacilitatedPaymentsNetworkInterface*,
              GetFacilitatedPaymentsNetworkInterface,
              (),
              (override));
  MOCK_METHOD(bool,
              ShowPixPaymentPrompt,
              (base::span<autofill::BankAccount> pix_account_suggestions,
               base::OnceCallback<void(bool, int64_t)>),
              (override));
};

class MockFacilitatedPaymentsNetworkInterface
    : public FacilitatedPaymentsNetworkInterface {
 public:
  MockFacilitatedPaymentsNetworkInterface()
      : FacilitatedPaymentsNetworkInterface(/*url_loader_factory=*/nullptr,
                                            /*identity_manager=*/nullptr,
                                            /*account_info_getter=*/nullptr) {}
  ~MockFacilitatedPaymentsNetworkInterface() override = default;

  MOCK_METHOD(
      void,
      InitiatePayment,
      (std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>,
       InitiatePaymentResponseCallback,
       const std::string&),
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
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();
    auto api_client = std::make_unique<MockFacilitatedPaymentsApiClient>();
    api_client_ = api_client.get();
    manager_ = std::make_unique<FacilitatedPaymentsManager>(
        driver_.get(), client_.get(), std::move(api_client),
        optimization_guide_decider_.get());
    manager_->is_test_ = true;

    // Using Autofill preferences since we use autofill's infra for syncing bank
    // accounts.
    pref_service_ = autofill::test::PrefServiceForTesting();
    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();
    personal_data_manager_->SetPrefService(pref_service_.get());
    personal_data_manager_->SetSyncServiceForTest(&sync_service_);
    ON_CALL(*client_, GetPersonalDataManager)
        .WillByDefault(testing::Return(personal_data_manager_.get()));

    ON_CALL(*client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(&payments_network_interface_));
  }

  void TearDown() override {
    api_client_ = nullptr;
    allowlist_decision_timer_.Stop();
    page_load_timer_.Stop();
    personal_data_manager_->payments_data_manager()
        .ClearAllServerDataForTesting();
    personal_data_manager_.reset();
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

  // Returns a bank account enabled for Pix with fake data.
  static autofill::BankAccount CreatePixBankAccount(int64_t instrument_id) {
    autofill::BankAccount bank_account(
        instrument_id, u"nickname", GURL("http://www.example.com"),
        u"bank_name", u"account_number",
        autofill::BankAccount::AccountType::kChecking);
    return bank_account;
  }

 protected:
  base::test::ScopedFeatureList features_;
  optimization_guide::OptimizationGuideDecision allowlist_result_;
  mojom::PixCodeDetectionResult pix_code_detection_result_;
  std::unique_ptr<MockOptimizationGuideDecider> optimization_guide_decider_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<MockFacilitatedPaymentsDriver> driver_;
  std::unique_ptr<MockFacilitatedPaymentsClient> client_;
  std::unique_ptr<FacilitatedPaymentsManager> manager_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  MockFacilitatedPaymentsNetworkInterface payments_network_interface_;

  // Owned by the `manager_`.
  raw_ptr<MockFacilitatedPaymentsApiClient> api_client_ = nullptr;

 private:
  // Number of attempts at checking the allowlist.
  int check_allowlist_attempt_count_;
  base::OneShotTimer allowlist_decision_timer_;
  base::OneShotTimer page_load_timer_;
  std::unique_ptr<PrefService> pref_service_;
  syncer::TestSyncService sync_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
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
          testing::ByRef(pix_code_detection_result_), std::string()));

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
      mojom::PixCodeDetectionResult::kPixCodeNotFound, std::string());

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
      .WillOnce(base::test::RunOnceCallback<0>(GetParam(), std::string()));

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
          testing::ByRef(pix_code_detection_result_), std::string()));

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
          testing::ByRef(pix_code_detection_result_), std::string()));

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
          testing::ByRef(pix_code_detection_result_), std::string()));

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
  manager_->ProcessPixCodeDetectionResult(GetParam(), std::string());

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

// If the facilitated payment API is not available, then the manager does not
// show the PIX payment prompt.
TEST_F(FacilitatedPaymentsManagerTest,
       NoPixPaymentPromptWhenApiClientNotAvailable) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/2));

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::_, testing::_)).Times(0);

  manager_->OnApiAvailabilityReceived(false);
}

// If the facilitated payment API is available, then the manager shows the PIX
// payment prompt.
TEST_F(FacilitatedPaymentsManagerTest,
       ShowsPixPaymentPromptWhenApiClientAvailable) {
  autofill::BankAccount pix_account1 =
      CreatePixBankAccount(/*instrument_id=*/1);
  autofill::BankAccount pix_account2 =
      CreatePixBankAccount(/*instrument_id=*/2);
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      pix_account1);
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      pix_account2);

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::UnorderedElementsAreArray(
                                                 {pix_account1, pix_account2}),
                                             testing::_));

  manager_->OnApiAvailabilityReceived(true);
}

// If the API is not available, request for risk data is not made.
TEST_F(FacilitatedPaymentsManagerTest,
       ApiClientNotAvailable_RiskDataNotLoaded_DoesNotTriggerLoadRiskData) {
  EXPECT_CALL(*client_, LoadRiskData(testing::_)).Times(0);

  manager_->OnApiAvailabilityReceived(false);
}

// If the API is available, and the risk data has already loaded from a previous
// call, request for risk data is not made.
TEST_F(FacilitatedPaymentsManagerTest,
       ApiClientAvailable_RiskDataLoaded_DoesNotTriggerLoadRiskData) {
  EXPECT_CALL(*client_, LoadRiskData(testing::_)).Times(0);

  manager_->OnRiskDataLoaded("seems pretty risky");
  manager_->OnApiAvailabilityReceived(true);
}

// If the API is available, and the risk data is empty, request for risk data is
// made.
TEST_F(FacilitatedPaymentsManagerTest,
       ApiClientAvailable_RiskDataNotLoaded_TriggersLoadRiskData) {
  EXPECT_CALL(*client_, LoadRiskData(testing::_));

  manager_->OnApiAvailabilityReceived(true);
}

// If the risk data is empty, then the PaymentNotOfferedReason histogram should
// be logged.
TEST_F(FacilitatedPaymentsManagerTest, PaymentNotOfferedReason_RiskDataEmpty) {
  base::HistogramTester histogram_tester;
  manager_->OnRiskDataLoaded("");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason",
      /*sample=*/PaymentNotOfferedReason::kRiskDataEmpty,
      /*expected_bucket_count=*/1);
}

// If a user has rejected the PIX payment prompt, then the manager does not
// retrieve a client token from the facilitated payments API client.
TEST_F(FacilitatedPaymentsManagerTest,
       DoesNotRetrieveClientTokenIfPixPaymentPromptRejected) {
  EXPECT_CALL(*api_client_, GetClientToken(testing::_)).Times(0);

  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/false,
                                     /*selected_instrument_id=*/-1);
}

// If a user has accepted the PIX payment prompt, then the manager retrieves a
// client token from the facilitated payments API client.
TEST_F(FacilitatedPaymentsManagerTest,
       RetrievesClientTokenIfPixPaymentPromptAccepted) {
  EXPECT_CALL(*api_client_, GetClientToken(testing::_));

  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/true,
                                     /*selected_instrument_id=*/-1);
}

// The GetClientToken async call is made after the user has accepted the payment
// prompt. This test verifies that the result and latency of the GetClientToken
// call is logged correctly.
TEST_F(FacilitatedPaymentsManagerTest,
       GetClientTokenHistogram_ClientTokenNotEmpty) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*api_client_, GetClientToken(testing::_));
  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/true,
                                     /*selected_instrument_id=*/-1);
  FastForwardBy(base::Seconds(2));

  manager_->OnGetClientToken(std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// The GetClientToken async call is made after the user has accepted the payment
// prompt. This test verifies that the result and latency of the GetClientToken
// call is logged correctly.
TEST_F(FacilitatedPaymentsManagerTest,
       GetClientTokenHistogram_ClientTokenEmpty) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*api_client_, GetClientToken(testing::_));
  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/true,
                                     /*selected_instrument_id=*/-1);
  FastForwardBy(base::Seconds(2));

  manager_->OnGetClientToken(std::vector<uint8_t>{});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.GetClientToken.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

TEST_F(FacilitatedPaymentsManagerTest,
       TriggerPixDetectionOnDomContentLoadedExpDisabled_Ukm) {
  features_.InitAndDisableFeature(kEnablePixDetectionOnDomContentLoaded);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kDetectionTriggeredOnDomContentLoadedName});

  // Verify that the UKM metrics are logged.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("DetectionTriggeredOnDomContentLoaded"),
            false);
}

TEST_F(FacilitatedPaymentsManagerTest,
       TriggerPixDetectionOnDomContentLoadedExpEnabled_Ukm) {
  features_.InitAndEnableFeature(kEnablePixDetectionOnDomContentLoaded);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeDetectionResult::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeDetectionResult::
           kDetectionTriggeredOnDomContentLoadedName});

  // Verify that the UKM metrics are logged.
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("DetectionTriggeredOnDomContentLoaded"),
            true);
}

TEST_F(FacilitatedPaymentsManagerTest, ResettingPreventsPayment) {
  manager_->initiate_payment_request_details_->risk_data_ =
      "seems pretty risky";
  manager_->initiate_payment_request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->initiate_payment_request_details_->billing_customer_number_ = 13;
  manager_->initiate_payment_request_details_->merchant_payment_page_url_ =
      GURL("https://foo.com/bar");
  manager_->initiate_payment_request_details_->instrument_id_ = 13;
  manager_->initiate_payment_request_details_->pix_code_ = "a valid code";

  EXPECT_TRUE(
      manager_->initiate_payment_request_details_->IsReadyForPixPayment());

  manager_->ResetForTesting();

  EXPECT_FALSE(
      manager_->initiate_payment_request_details_->IsReadyForPixPayment());
}

// A test fixture for the facilitated payment manager with the
// kEnablePixPayments feature flag disabled.
class FacilitatedPaymentsManagerWithPixPaymentsDisabledTest
    : public FacilitatedPaymentsManagerTest {
 public:
  FacilitatedPaymentsManagerWithPixPaymentsDisabledTest() {
    features_.InitAndDisableFeature(kEnablePixPayments);
  }

  ~FacilitatedPaymentsManagerWithPixPaymentsDisabledTest() override = default;
};

// If the kEnablePixPayments flag is disabled, and if a valid PIX code is
// detected for a user with PIX accounts, the manager does not check whether the
// facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsDisabledTest,
       ValidPixCodeDetectionResult_HasPixAccounts_ApiClientNotTriggered) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));

  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());
}

// A test fixture for the facilitated payment manager with the
// kEnablePixPayments feature flag enabled.
class FacilitatedPaymentsManagerWithPixPaymentsEnabledTest
    : public FacilitatedPaymentsManagerTest {
 public:
  FacilitatedPaymentsManagerWithPixPaymentsEnabledTest() {
    features_.InitAndEnableFeature(kEnablePixPayments);
  }

  ~FacilitatedPaymentsManagerWithPixPaymentsEnabledTest() override = default;
};

// If a valid PIX code is detected, and the user has PIX accounts, the manager
// checks whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       ValidPixCodeDetectionResult_HasPixAccounts_ApiClientTriggered) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));

  EXPECT_CALL(*api_client_, IsAvailable(testing::_));

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound,
      "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F");

  // The DataDecoder (utility process) validates the PIX code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

// If the renderer indicates that a valid PIX code is detected, but sends an
// invalid code to the browser, the manager does not proceed to check whether
// the API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       ValidPixCodeDetectionResult_InvalidPixCodeString_ApiClientNotTriggered) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));

  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());

  // The DataDecoder (utility process) validates the PIX code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

// When an invalid PIX code is detected, the manager does not check whether the
// facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       InvalidPixCodeDetectionResultDoesNotTriggerApiClient) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));

  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kInvalidPixCodeFound, std::string());
}

// If the PIX code has been validated, then the manager checks for API
// availability.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PixCodeValidated_ApiClientTriggered) {
  EXPECT_CALL(*api_client_, IsAvailable(testing::_));

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               /*is_pix_code_valid=*/true);
}

// If the PIX code validation in the utility process has returned `false`, then
// the manager does not check the API for availability.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PixCodeValidationFailed_NoApiClientTriggered) {
  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               /*is_pix_code_valid=*/false);
}

// If the PIX code validation in the utility process has returned `false`, then
// the PaymentNotOfferedReason histogram should be logged.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PaymentNotOfferedReason_CodeValidatorReturnsFalse) {
  base::HistogramTester histogram_tester;
  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               /*is_pix_code_valid=*/false);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason",
      /*sample=*/PaymentNotOfferedReason::kInvalidCode,
      /*expected_bucket_count=*/1);
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then the manager does not check the API for
// availability.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PixCodeValidatorTerminatedUnexpectedly_NoApiClientTriggered) {
  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(),
      /*is_pix_code_valid=*/base::unexpected(
          "Data Decoder terminated unexpectedly"));
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then the PaymentNotOfferedReason histogram should be
// logged.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PaymentNotOfferedReason_CodeValidatorFailed) {
  base::HistogramTester histogram_tester;
  manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(),
      /*is_pix_code_valid=*/base::unexpected(
          "Data Decoder terminated unexpectedly"));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason",
      /*sample=*/PaymentNotOfferedReason::kCodeValidatorFailed,
      /*expected_bucket_count=*/1);
}

// If the user doesn't have any linked PIX accounts, the manager does not check
// whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       AbsenceOfPixAccountsDoesNotTriggerApiClient) {
  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());
}

// If personal data manager is unavailable, the manager does not check
// whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       UnavailabilityOfPdmDoesNotTriggerApiClient) {
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));
  ON_CALL(*client_, GetPersonalDataManager)
      .WillByDefault(testing::Return(nullptr));

  EXPECT_CALL(*api_client_, IsAvailable(testing::_)).Times(0);

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound, std::string());
}

// If a valid PIX code is detected, and the user has PIX accounts, and API
// client is available, then the manager will show a UI prompt for selecting a
// PIX account.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       ValidPixDetectionResultToPixPaymentPromptShown) {
  autofill::BankAccount pix_account1 =
      CreatePixBankAccount(/*instrument_id=*/1);
  autofill::BankAccount pix_account2 =
      CreatePixBankAccount(/*instrument_id=*/2);
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      pix_account1);
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      pix_account2);
  ON_CALL(*api_client_, IsAvailable)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::UnorderedElementsAreArray(
                                                 {pix_account1, pix_account2}),
                                             testing::_));

  manager_->ProcessPixCodeDetectionResult(
      mojom::PixCodeDetectionResult::kValidPixCodeFound,
      "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F");

  // The DataDecoder (utility process) validates the PIX code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

// Test that SendInitiatePaymentRequest initiates payment using the
// FacilitatedPaymentsNetworkInterface.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       SendInitiatePaymentRequest) {
  EXPECT_CALL(payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_));

  manager_->SendInitiatePaymentRequest();
}

// The `IsAvailable` async call is made after a valid Pix code has been
// detected. This test verifies that the result and latency are logged after the
// async call is completed.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       ApiAvailabilityHistogram) {
  base::HistogramTester histogram_tester;
  personal_data_manager_->payments_data_manager().AddMaskedBankAccountForTest(
      CreatePixBankAccount(1));
  EXPECT_CALL(*api_client_, IsAvailable(testing::_));
  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               /*is_pix_code_valid=*/true);
  FastForwardBy(base::Seconds(2));

  manager_->OnApiAvailabilityReceived(true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// The `IsAvailable` async call is made after a valid Pix code has been
// detected. This test verifies that if the api available result is false, the
// PaymentNotOfferedReason histogram is logged.
TEST_F(FacilitatedPaymentsManagerWithPixPaymentsEnabledTest,
       PaymentNotOfferedReason_ApiNotAvailable) {
  base::HistogramTester histogram_tester;

  manager_->OnApiAvailabilityReceived(false);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason",
      /*sample=*/PaymentNotOfferedReason::kApiNotAvailable,
      /*expected_bucket_count=*/1);
}

}  // namespace payments::facilitated
