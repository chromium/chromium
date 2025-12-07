// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/model/secure_payload.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

// Returns a bank account enabled for Pix with fake data.
autofill::BankAccount CreatePixBankAccount(int64_t instrument_id) {
  autofill::BankAccount bank_account(
      instrument_id, u"nickname", GURL("http://www.example.com"), u"bank_name",
      u"account_number", autofill::BankAccount::AccountType::kChecking);
  return bank_account;
}

// Returns an account info that has all the details a logged in account should
// have.
CoreAccountInfo CreateLoggedInAccountInfo() {
  CoreAccountInfo account;
  account.email = "foo@bar.com";
  account.gaia = GaiaId("foo-gaia-id");
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  return account;
}

SecurePayload CreateSecurePayload() {
  SecurePayload secure_payload;
  secure_payload.action_token = {'A', 'c', 't', 'i', 'o', 'n'};
  secure_payload.secure_data.emplace_back(1, "value_1");
  secure_payload.secure_data.emplace_back(2, "value_2");
  return secure_payload;
}

}  // namespace

class PixManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    optimization_guide_decider_ =
        std::make_unique<optimization_guide::MockOptimizationGuideDecider>();
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();
    mock_device_delegate_ = std::make_unique<MockDeviceDelegate>();
    pix_manager_ = std::make_unique<PixManager>(
        client_.get(), /*api_client_creator=*/
        base::BindRepeating(&MockFacilitatedPaymentsApiClient::CreateApiClient),
        optimization_guide_decider_.get());

    // Using Autofill preferences since we use autofill's infra for syncing
    // bank accounts.
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    ON_CALL(*client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(payments_data_manager_.get()));
    ON_CALL(*client_, IsInLandscapeMode).WillByDefault(testing::Return(false));
    // By default, assume that the tab is started in the browser and not a
    // Chrome custom tab.
    ON_CALL(*client_, IsInChromeCustomTabMode())
        .WillByDefault(testing::Return(false));
  }

  void TearDown() override {
    payments_data_manager_->ClearAllServerDataForTesting();
    payments_data_manager_.reset();
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
    task_environment_.RunUntilIdle();
  }

  MockFacilitatedPaymentsApiClient& GetApiClient() {
    return *static_cast<MockFacilitatedPaymentsApiClient*>(
        pix_manager_->GetApiClient());
  }

 protected:
  std::unique_ptr<optimization_guide::MockOptimizationGuideDecider>
      optimization_guide_decider_;
  std::unique_ptr<MockFacilitatedPaymentsClient> client_;
  std::unique_ptr<PixManager> pix_manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<MockDeviceDelegate> mock_device_delegate_;

 private:
  syncer::TestSyncService sync_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

// Since the Pix account linking flow is triggered from with the Pix payflow,
// creating a new class with account linking flag enabled to verify the payflow
// isn't affected. When account linking is fully launched, this class can be
// deprecated.
class PixManagerTestWithAccountLinkingEnabled : public PixManagerTest {
 public:
  void SetUp() override {
    PixManagerTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kEnablePixAccountLinking);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// If the facilitated payment API is not available, then the manager does not
// show the Pix payment prompt.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       NoPixPaymentPromptWhenApiClientNotAvailable) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/2));

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::_, testing::_)).Times(0);

  pix_manager_->OnApiAvailabilityReceived(/*start_time=*/base::TimeTicks::Now(),
                                          /*is_api_available=*/false);
}

// If the facilitated payment API is available, then the manager shows the Pix
// payment prompt.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ShowsPixPaymentPromptWhenApiClientAvailable) {
  autofill::BankAccount pix_account1 =
      CreatePixBankAccount(/*instrument_id=*/1);
  autofill::BankAccount pix_account2 =
      CreatePixBankAccount(/*instrument_id=*/2);
  payments_data_manager_->AddMaskedBankAccountForTest(pix_account1);
  payments_data_manager_->AddMaskedBankAccountForTest(pix_account2);

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::UnorderedElementsAreArray(
                                                 {pix_account1, pix_account2}),
                                             testing::_));

  pix_manager_->OnApiAvailabilityReceived(/*start_time=*/base::TimeTicks::Now(),
                                          /*is_api_available=*/true);
}

// If the user selects a Pix account on the payment prompt,
// 1. Request for risk data is made.
// 2. Progress screen is shown.
// 3. Histogram is logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled, OnPixAccountSelected) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, ShowProgressScreen());
  EXPECT_CALL(*client_, LoadRiskData(testing::_));

  pix_manager_->OnPixAccountSelected(base::TimeTicks::Now() - base::Seconds(2),
                                     /*selected_instrument_id=*/0);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelected.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"), true);
}

// Verify risk data metrics are logged when risk data is fetched successfully.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       RiskDataNotEmpty_HistogramsLogged) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2),
                                 "seems pretty risky");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.LoadRiskData.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Verify risk data metrics are logged when risk data is empty.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       RiskDataEmpty_HistogramsLogged) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2), "");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.LoadRiskData.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// If the risk data is empty, then the PayflowExitedReason histogram should
// be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_RiskDataEmpty) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnRiskDataLoaded(base::TimeTicks::Now(), "");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kRiskDataNotAvailable,
      /*expected_bucket_count=*/1);
}

// If the risk data is empty, then the manager does not retrieve a client token
// from the facilitated payments API client.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_)).Times(0);
  EXPECT_CALL(*client_, ShowErrorScreen());

  pix_manager_->OnRiskDataLoaded(/*start_time=*/base::TimeTicks::Now(),
                                 /*risk_data=*/"");
}

// If the risk data is not empty, then the manager retrieves a client token from
// the facilitated payments API client.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       RiskDataNotEmpty_GetClientTokenCalled) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_));

  pix_manager_->OnRiskDataLoaded(/*start_time=*/base::TimeTicks::Now(),
                                 /*risk_data=*/"seems pretty risky");
}

// Verify that the result and latency of the GetClientToken call is logged
// correctly.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       LogGetClientTokenResultAndLatency) {
  for (bool get_client_token_result : {true, false}) {
    base::HistogramTester histogram_tester;

    pix_manager_->OnGetClientToken(
        /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
        get_client_token_result ? std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'}
                                : std::vector<uint8_t>{});

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Pix.GetClientToken.",
                      get_client_token_result ? "Success" : "Failure",
                      ".Latency"}),
        /*sample=*/2000,
        /*expected_bucket_count=*/1);
  }
}

// If the client token is not available, then the PayflowExitedReason histogram
// should be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_ClientTokenNotAvailable) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnGetClientToken(/*start_time=*/base::TimeTicks::Now(),
                                 std::vector<uint8_t>{});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kClientTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       OnGetClientToken_ClientTokenEmpty_ErrorScreenShown) {
  EXPECT_CALL(*client_, ShowErrorScreen());

  pix_manager_->OnGetClientToken(/*start_time=*/base::TimeTicks::Now(),
                                 std::vector<uint8_t>{});
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, ResettingPreventsPayment) {
  pix_manager_->initiate_payment_request_details_->risk_data_ =
      "seems pretty risky";
  pix_manager_->initiate_payment_request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  pix_manager_->initiate_payment_request_details_->billing_customer_number_ =
      13;
  pix_manager_->initiate_payment_request_details_
      ->merchant_payment_page_hostname_ = "foo.com";
  pix_manager_->initiate_payment_request_details_->instrument_id_ = 13;
  pix_manager_->initiate_payment_request_details_->pix_code_ = "a valid code";

  EXPECT_TRUE(
      pix_manager_->initiate_payment_request_details_->IsReadyForPixPayment());

  pix_manager_->Reset();

  EXPECT_FALSE(
      pix_manager_->initiate_payment_request_details_->IsReadyForPixPayment());
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, CopyTrigger_LogPixCodeCopied) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  pix_manager_->OnPixCodeCopiedToClipboard(
      url, origin, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.PixCodeCopied",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeCopied::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeCopied::kPixCodeCopiedName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PixCodeCopied"), true);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       CopyTrigger_UrlInAllowlist_PixValidationTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));
  // If Pix validation is run, then IsAvailable should get called once.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  pix_manager_->OnPixCodeCopiedToClipboard(
      url, origin, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());

  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       CopyTrigger_UrlNotInAllowlist_PixValidationNotTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  // If Pix validation is not run, then IsAvailable shouldn't get called.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  pix_manager_->OnPixCodeCopiedToClipboard(
      url, origin, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       CopyTrigger_UrlNotInAllowlist_PayflowExitedHistogramLogged) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  pix_manager_->OnPixCodeCopiedToClipboard(
      url, origin, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kMerchantNotAllowlisted,
      /*expected_bucket_count=*/1);
}

TEST_F(
    PixManagerTestWithAccountLinkingEnabled,
    CopyTrigger_UrlNotInAllowlist_AllowlistCheckDisabled_PixValidationTriggered) {
  base::test::ScopedFeatureList feature_list(
      kDisableFacilitatedPaymentsMerchantAllowlist);
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  // Verify that the allowlist check never happens.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(0);
  // If Pix validation is run, then IsAvailable should get called once.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  pix_manager_->OnPixCodeCopiedToClipboard(
      url, origin, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       TestPayFlowCanBeTriggeredOnlyOncePerPageLoad) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  // Mock allowlist check result.
  EXPECT_CALL(*optimization_guide_decider_,
              CanApplyOptimization(
                  testing::Eq(url), testing::_,
                  testing::Matcher<optimization_guide::OptimizationMetadata*>(
                      testing::Eq(nullptr))))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  // Even if there are multiple copy events, the payflow should be initiated
  // only once. This can be verified with a single IsAvailable call.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  std::string pix_code =
      "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F";
  pix_manager_->OnPixCodeCopiedToClipboard(url, origin, pix_code,
                                           ukm::UkmRecorder::GetNewSourceID());
  pix_manager_->OnPixCodeCopiedToClipboard(url, origin, pix_code,
                                           ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

// The manager checks for API availability after validating the Pix code if all
// checks pass. The account linking flow shouldn't be triggered.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ApiClientTriggeredAfterPixCodeValidation) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then neither payflow nor the account linking flow is
// initiated.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       CodeValidatorFailed_PixFlowsAbandoned) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*is_pix_code_valid=*/
      base::unexpected("Data Decoder terminated unexpectedly"));
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then the PayflowExitedReason histogram should be
// logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_CodeValidatorFailed) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*is_pix_code_valid=*/
      base::unexpected("Data Decoder terminated unexpectedly"));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kCodeValidatorFailed,
      /*expected_bucket_count=*/1);
}

// If the Pix code validation in the utility process has returned `false`, then
// neither payflow nor the account linking flow is initiated.
TEST_F(PixManagerTestWithAccountLinkingEnabled, InvalidCode_PixFlowsAbandoned) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kInvalid);
}

// If the Pix code validation in the utility process has returned `false`, then
// the PayflowExitedReason histogram should be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_InvalidCode) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kInvalid);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kInvalidCode,
      /*expected_bucket_count=*/1);
}

// If the user doesn't have any linked Pix account, the PayflowExitedReason
// histogram should be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_NoLinkedAccount) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kNoLinkedAccount,
      /*expected_bucket_count=*/1);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_StaticCode_FeatureDisabled_PixFlowsAbandoned) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  feature_list.InitAndDisableFeature(kEnableStaticQrCodeForPix);
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kStatic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kStaticCode,
      /*expected_bucket_count=*/1);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_StaticCode_ApiClientAvailabilityChecked) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableStaticQrCodeForPix);
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(1);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kStatic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PaymentCodeValidation.Result",
      /*sample=*/PixCodeValidationResult::kStatic,
      /*expected_bucket_count=*/1);
}

// If payments data manager is unavailable, neither the payflow nor the account
// linking flow is initiated.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       NoPaymentsDataManager_PixFlowsAbandoned) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  ON_CALL(*client_, GetPaymentsDataManager)
      .WillByDefault(testing::Return(nullptr));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// If the payments autofill pref is disabled, neither the payflow nor the
// account linking flow is initiated.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PaymentsAutofillTurnedOff_PixFlowsAbandoned) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  // Disable payment methods pref.
  autofill::prefs::SetAutofillPaymentMethodsEnabled(pref_service_.get(), false);

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow(testing::_)).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// If the user has turned off autofilling payment methods, the
// PayflowExitedReason histogram should be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_PaymentsAutofillTurnedOff) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  // Disable payment methods pref.
  autofill::prefs::SetAutofillPaymentMethodsEnabled(pref_service_.get(), false);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kAutofillPaymentMethodsDisabled,
      /*expected_bucket_count=*/1);
}

// If the user has opted out of Pix, neither the payflow nor the
// account linking flow is initiated.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       UserOptedOut_PixFlowsAbandoned) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  autofill::prefs::SetFacilitatedPaymentsPix(pref_service_.get(), false);

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow(testing::_)).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// If the user has opted out of the Pix flow, the PayflowExitedReason
// histogram should be logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_UserOptedOut) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  autofill::prefs::SetFacilitatedPaymentsPix(pref_service_.get(), false);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kUserOptedOut,
      /*expected_bucket_count=*/1);
}

// If the user doesn't have any linked Pix account, the account linking flow
// should be initialized.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       NoLinkedAccount_AccountLinkingFlowTriggered) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);
  EXPECT_CALL(*client_, InitPixAccountLinkingFlow(testing::_));

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// If the account linking flag is disabled, the account linking flow shouldn't
// be initialized.
TEST_F(
    PixManagerTestWithAccountLinkingEnabled,
    NoLinkedAccount_AccountLinkingFlagDisabled_AccountLinkingFlowNotTriggered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnablePixAccountLinking);

  EXPECT_CALL(*client_, InitPixAccountLinkingFlow(testing::_)).Times(0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

// Verify that the API check result and latency are logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       LogApiAvailabilityCheckResultAndLatency) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnApiAvailabilityReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      /*is_api_available=*/true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// The `IsAvailable` async call is made after a valid Pix code has been
// detected. This test verifies that if the api available result is false, the
// PayflowExitedReason histogram is logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PayflowExitedReason_ApiClientNotAvailable) {
  base::HistogramTester histogram_tester;

  pix_manager_->OnApiAvailabilityReceived(/*start_time=*/base::TimeTicks::Now(),
                                          /*is_api_available=*/false);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kApiClientNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that when Chrome fails to invoke purchase action, the error screen is
// shown.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       OnPurchaseActionResult_CouldNotInvoke_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, ShowErrorScreen);

  pix_manager_->OnPurchaseActionResult(/*start_time=*/base::TimeTicks::Now(),
                                       PurchaseActionResult::kCouldNotInvoke);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kPurchaseActionCouldNotBeInvoked,
      /*expected_bucket_count=*/1);
}

// Test that when Chrome is successful in invoking the purchase action, the UI
// screen is dismissed.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       OnPurchaseActionResult_ResultOk_UiScreenDismissed) {
  // `DismissPrompt` is called once when the purchase action result is
  // received, and again when the test fixture destroys the `pix_manager_`.
  EXPECT_CALL(*client_, DismissPrompt).Times(2);

  pix_manager_->OnPurchaseActionResult(/*start_time=*/base::TimeTicks::Now(),
                                       PurchaseActionResult::kResultOk);
}

// Test that when Chrome is successful in invoking the purchase action, the UI
// screen is dismissed.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       OnPurchaseActionResult_ResultCanceled_UiScreenDismissed) {
  // `DismissPrompt` is called once when the purchase action result is
  // received, and again when the test fixture destroys the `pix_manager_`.
  EXPECT_CALL(*client_, DismissPrompt).Times(2);

  pix_manager_->OnPurchaseActionResult(/*start_time=*/base::TimeTicks::Now(),
                                       PurchaseActionResult::kResultCanceled);
}

// Test that when an InitiatePurchaseAction request is sent, the attempt is
// logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       LogInitiatePurchaseActionAttempt) {
  base::HistogramTester histogram_tester;
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now(),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that when an InitiatePurchaseAction response is received, the result and
// latency of the invoke purchase action is logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       LogInitiatePurchaseActionResultAndLatency) {
  size_t index = 0;
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;
    ON_CALL(*client_, GetCoreAccountInfo)
        .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));
    EXPECT_CALL(GetApiClient(), InvokePurchaseAction);
    auto response_details =
        std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
    response_details->secure_payload_ = CreateSecurePayload();
    pix_manager_->OnInitiatePaymentResponseReceived(
        /*start_time=*/base::TimeTicks::Now(),
        autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
        std::move(response_details));

    pix_manager_->OnPurchaseActionResult(
        /*start_time=*/base::TimeTicks::Now() - base::Seconds(2), result);

    std::string result_string;
    switch (result) {
      case PurchaseActionResult::kResultOk:
        result_string = "Succeeded";
        break;
      case PurchaseActionResult::kCouldNotInvoke:
        result_string = "Failed";
        break;
      case PurchaseActionResult::kResultCanceled:
        result_string = "Abandoned";
        break;
    }
    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.",
                      result_string, ".Latency"}),
        /*sample=*/2000,
        /*expected_count=*/1);
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
            kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"),
              static_cast<uint8_t>(result));
  }
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       LogTransactionResultAndLatency) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  // Simulate Pix code being copied. The transaction latency is computed from
  // this point.
  pix_manager_->OnPixCodeCopiedToClipboard(url, origin, std::string(),
                                           ukm::UkmRecorder::GetNewSourceID());
  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));

  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    std::string result_string;
    switch (result) {
      case PurchaseActionResult::kResultOk:
        result_string = "Succeeded";
        break;
      case PurchaseActionResult::kCouldNotInvoke:
        result_string = "Failed";
        break;
      case PurchaseActionResult::kResultCanceled:
        result_string = "Abandoned";
        break;
    }

    pix_manager_->OnPurchaseActionResult(
        /*start_time=*/base::TimeTicks::Now(), result);

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.Transaction.", result_string,
                      ".Latency"}),
        /*sample=*/2000,
        /*expected_count=*/1);
  }
}

// Verify that the API client is initialized lazily, so it does not take up
// space in memory unless it's being used.
TEST_F(PixManagerTestWithAccountLinkingEnabled, ApiClientInitializedLazily) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_EQ(nullptr, pix_manager_->api_client_.get());

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  EXPECT_NE(nullptr, pix_manager_->api_client_.get());
}

// Verify that a failure to lazily initialize the API client is not fatal.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       HandlesFailureToLazilyInitializeApiClient) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  pix_manager_->api_client_creator_.Reset();

  EXPECT_EQ(nullptr, pix_manager_->api_client_.get());

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  EXPECT_EQ(nullptr, pix_manager_->api_client_.get());
}

// Test class for devices being used in the landscape mode.
class PixManagerTestInLandscapeMode : public PixManagerTest,
                                      public testing::WithParamInterface<bool> {
 public:
  PixManagerTestInLandscapeMode() {
    scoped_feature_list_.InitWithFeatureState(kEnablePixPaymentsInLandscapeMode,
                                              GetParam());
  }

  void SetUp() override {
    PixManagerTest::SetUp();
    ON_CALL(*client_, IsInLandscapeMode).WillByDefault(testing::Return(true));
  }

  bool IsPaymentEnabledInLandscapeMode() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, PixManagerTestInLandscapeMode, testing::Bool());

TEST_P(PixManagerTestInLandscapeMode, PixPayflowBlockedWhenFlagDisabled) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  // In landscape mode, checking the API client's availability (which is part of
  // Pix payflow) is only done if the `EnablePixPaymentsInLandscapeMode` flag is
  // enabled.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_))
      .Times(IsPaymentEnabledInLandscapeMode() ? 1 : 0);

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

TEST_P(PixManagerTestInLandscapeMode,
       PayflowExitedReason_LandscapeScreenOrientation) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  // In landscape mode, if the `EnablePixPaymentsInLandscapeMode` flag is
  // disabled, Pix payment is not offered, and a histogram should be logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kLandscapeScreenOrientation,
      /*expected_bucket_count=*/IsPaymentEnabledInLandscapeMode() ? 0 : 1);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, ShowPixPaymentPrompt) {
  // Verify the default UI state.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the payment prompt, it asks the
  // client.
  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::_, testing::_));

  const std::vector<autofill::BankAccount> bank_accounts = {
      autofill::test::CreatePixBankAccount(100L)};
  pix_manager_->ShowPixPaymentPrompt(std::move(bank_accounts),
                                     base::DoNothing());

  // Verify that the UI state is updated.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kFopSelector);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, ShowProgressScreen) {
  // Verify the default UI state.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the progress screen, it asks the
  // client.
  EXPECT_CALL(*client_, ShowProgressScreen);

  pix_manager_->ShowProgressScreen();

  // Verify that the UI state is updated.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kProgressScreen);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, ShowErrorScreen) {
  // Verify the default UI state.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the error screen, it asks the
  // client.
  EXPECT_CALL(*client_, ShowErrorScreen);

  pix_manager_->ShowErrorScreen();

  // Verify that the UI state is updated.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kErrorScreen);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled, DismissPrompt) {
  // Verify that when the feature wants to dismiss the UI screen, it asks the
  // client. The second call is from test teardown.
  EXPECT_CALL(*client_, DismissPrompt).Times(2);

  pix_manager_->DismissPrompt();

  // Verify that the UI state is updated.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);
}

// Test that when the Pix FOP selector is shown, related Pix metrics are logged.
TEST_F(PixManagerTestWithAccountLinkingEnabled,
       PixFopSelectorShown_HistogramsLogged) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  // Simulate Pix code being copied. The latency is computed from this point.
  pix_manager_->OnPixCodeCopiedToClipboard(url, origin, std::string(),
                                           ukm::UkmRecorder::GetNewSourceID());
  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));
  // Simulate that the FOP selector was shown successfully.
  std::vector<autofill::BankAccount> bank_accounts = {
      autofill::test::CreatePixBankAccount(100L)};
  pix_manager_->ShowPixPaymentPrompt(std::move(bank_accounts),
                                     base::DoNothing());
  pix_manager_->OnUiScreenEvent(UiEvent::kNewScreenShown);

  // Verify that when the Pix FOP selector is shown, related metrics are
  // logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelectorShown.LatencyAfterCopy",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ProgressScreenAutoDismissedAfterInvokingPurchaseAction) {
  // When purchase action is invoked, the progress screen would be showing.
  pix_manager_->ShowProgressScreen();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now(),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  // The progress screen is persisted for a short duration after invoking the
  // purchase action for a smooth transition to the platform screen.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kProgressScreen);

  FastForwardBy(base::Seconds(2));

  // The progress screen should be dismissed after a short delay.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ChromeCustomTabWithGboardAsDefaultIme_PixFlowNotTriggered) {
  ON_CALL(*client_, IsInChromeCustomTabMode())
      .WillByDefault(testing::Return(true));
  ON_CALL(*client_, GetDeviceDelegate)
      .WillByDefault(testing::Return(mock_device_delegate_.get()));
  ON_CALL(*mock_device_delegate_, IsPixSupportAvailableViaGboard)
      .WillByDefault(testing::Return(true));
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kCctWithGboardAsDefaultIme,
      /*expected_bucket_count=*/1);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ChromeCustomTabWithGboardNotAsDefaultIme_PixFlowTriggered) {
  ON_CALL(*client_, IsInChromeCustomTabMode())
      .WillByDefault(testing::Return(true));
  ON_CALL(*client_, GetDeviceDelegate)
      .WillByDefault(testing::Return(mock_device_delegate_.get()));
  ON_CALL(*mock_device_delegate_, IsPixSupportAvailableViaGboard)
      .WillByDefault(testing::Return(false));
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  pix_manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*pix_qr_code_type=*/mojom::PixQrCodeType::kDynamic);
}

TEST_F(PixManagerTestWithAccountLinkingEnabled,
       ErrorScreenNotAutoDismissedAfterInvokingPurchaseAction) {
  // When purchase action is invoked, the progress screen would be showing.
  pix_manager_->ShowProgressScreen();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now(),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  // If the purchase action could not be invoked, the `PurchaseActionResult` is
  // returned immediately. The error screen is shown.
  pix_manager_->OnPurchaseActionResult(/*start_time=*/base::TimeTicks::Now(),
                                       PurchaseActionResult::kCouldNotInvoke);
  FastForwardBy(base::Seconds(1));

  // The error screen shouldn't be auto-dismissed.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kErrorScreen);
}

class PixManagerTestForUiScreens : public PixManagerTest,
                                   public testing::WithParamInterface<UiState> {
 public:
  void SetUp() override {
    PixManagerTest::SetUp();

    // Default state.
    EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);

    switch (GetParam()) {
      case UiState::kFopSelector: {
        const std::vector<autofill::BankAccount> bank_accounts = {
            autofill::test::CreatePixBankAccount(100L)};
        pix_manager_->ShowPixPaymentPrompt(std::move(bank_accounts),
                                           base::DoNothing());
        break;
      }
      case UiState::kProgressScreen:
        pix_manager_->ShowProgressScreen();
        break;
      case UiState::kErrorScreen:
        pix_manager_->ShowErrorScreen();
        break;
      case UiState::kHidden:
        NOTREACHED();
    }
  }

  UiState ui_state() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(PixManagerTestWithAccountLinkingEnabled,
                         PixManagerTestForUiScreens,
                         testing::Values(UiState::kFopSelector,
                                         UiState::kProgressScreen,
                                         UiState::kErrorScreen));

// Test that when a new screen is shown, UI state reflects the current UI being
// shown.
TEST_P(PixManagerTestForUiScreens, NewScreenShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  pix_manager_->OnUiScreenEvent(UiEvent::kNewScreenShown);

  // Verify feature has updated the UI state.
  EXPECT_EQ(pix_manager_->ui_state_, ui_state());
  // Verify that the histogram is logged.
  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.UiScreenShown",
                                      /*sample=*/ui_state(),
                                      /*expected_bucket_count=*/1);
  if (ui_state() == UiState::kFopSelector) {
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
    EXPECT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
  }
}

// Test that when a new screen could not be shown, UI state is updated.
TEST_P(PixManagerTestForUiScreens, NewScreenCouldNotBeShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen could not be shown.
  pix_manager_->OnUiScreenEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed, but it was not due to a user action,
// the feature updates the UI state.
TEST_P(PixManagerTestForUiScreens, ScreenClosedNotByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  pix_manager_->OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed, but it was not due to a user action.
  pix_manager_->OnUiScreenEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed by the user, the feature updates the
// UI state.
TEST_P(PixManagerTestForUiScreens, ScreenClosedByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  pix_manager_->OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed by the user.
  pix_manager_->OnUiScreenEvent(UiEvent::kScreenClosedByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(pix_manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
  if (ui_state() == UiState::kFopSelector) {
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("Result"), false);
  }
}

// Test the PixManager works with the FacilitatedPaymentsNetworkInterface
// correctly.
class PixManagerPaymentsNetworkInterfaceTest : public PixManagerTest {
 public:
  PixManagerPaymentsNetworkInterfaceTest() {
    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), *payments_data_manager_);
  }

  void SetUp() override {
    PixManagerTest::SetUp();
    ON_CALL(*client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));
  }

 protected:
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
};

// Test that SendInitiatePaymentRequest initiates payment using the
// FacilitatedPaymentsNetworkInterface.
TEST_F(PixManagerPaymentsNetworkInterfaceTest, SendInitiatePaymentRequest) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_));

  pix_manager_->SendInitiatePaymentRequest();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` call has failure
// result, purchase action is not invoked. Instead, an error message is
// shown.
TEST_F(PixManagerPaymentsNetworkInterfaceTest,
       OnInitiatePaymentResponseReceived_FailureResponse) {
  base::HistogramTester histogram_tester;
  pix_manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kInitiatePaymentFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` has empty action
// token, purchase action is not invoked. Instead, an error message is shown.
TEST_F(PixManagerPaymentsNetworkInterfaceTest,
       OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  pix_manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that if the core account is std::nullopt, purchase action is not
// invoked. Instead, an error message is shown.
TEST_F(PixManagerPaymentsNetworkInterfaceTest,
       OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  pix_manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(std::nullopt));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

// Test that if the user is logged out, purchase action is not invoked.
// Instead, an error message is shown.
TEST_F(PixManagerPaymentsNetworkInterfaceTest,
       OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  pix_manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CoreAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

// Test that the purchase action is invoked after receiving a success response
// from the `FacilitatedPaymentsNetworkInterface::InitiatePayment` call.
TEST_F(PixManagerPaymentsNetworkInterfaceTest,
       OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered) {
  base::HistogramTester histogram_tester;
  pix_manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  pix_manager_->OnInitiatePaymentResponseReceived(
      /*start_time=*/base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Test that refreshing the page will cancel pending initiate payment request
// callback.
TEST_F(PixManagerPaymentsNetworkInterfaceTest, Reset) {
  EXPECT_CALL(*payments_network_interface_, InitiatePayment);

  pix_manager_->SendInitiatePaymentRequest();
  pix_manager_->Reset();

  EXPECT_FALSE(pix_manager_->weak_ptr_factory_.HasWeakPtrs());
}

}  // namespace payments::facilitated
