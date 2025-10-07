// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/payment_link_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/payment_link_manager_test_api.h"
#include "components/facilitated_payments/core/browser/strike_databases/payment_link_suggestion_strike_database.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {
namespace {

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
  return secure_payload;
}

}  // namespace

class PaymentLinkManagerTest : public testing::Test {
 public:
  PaymentLinkManagerTest()
      : payment_link_manager_(std::make_unique<PaymentLinkManager>(
            &client_, /*api_client_creator=*/
            base::BindRepeating(
                &MockFacilitatedPaymentsApiClient::CreateApiClient),
            &optimization_guide_decider_)) {
    // Using Autofill preferences since we use autofill's infra for syncing
    // eWallets.
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_.SetPrefService(pref_service_.get());
    payments_data_manager_.SetSyncServiceForTest(&sync_service_);
    test_strike_database_ = std::make_unique<autofill::TestStrikeDatabase>();
    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), payments_data_manager_);
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(&payments_data_manager_));
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));
    ON_CALL(client_, IsInLandscapeMode).WillByDefault(testing::Return(false));
    ON_CALL(client_, IsFoldable).WillByDefault(testing::Return(false));
    ON_CALL(client_, GetCoreAccountInfo)
        .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));
    ON_CALL(client_, GetStrikeDatabase)
        .WillByDefault(testing::Return(test_strike_database_.get()));
    ON_CALL(
        optimization_guide_decider_,
        CanApplyOptimization(
            testing::_,
            testing::Eq(optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST),
            testing::A<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(testing::Return(
            optimization_guide::OptimizationGuideDecision::kTrue));
    ON_CALL(GetApiClient(), IsAvailableSync())
        .WillByDefault(testing::Return(true));
    test_api(*payment_link_manager_)
        .set_scheme(PaymentLinkValidator::Scheme::kShopeePay);

    // `initiate_payment_request_details_` is lazy initialized in the
    // implementation. Initialize it here so tests depending on it won't crash.
    test_api(*payment_link_manager_)
        .set_initiate_payment_request_details(
            std::make_unique<
                FacilitatedPaymentsInitiatePaymentRequestDetails>());
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
  }

  MockFacilitatedPaymentsApiClient& GetApiClient() {
    return *static_cast<MockFacilitatedPaymentsApiClient*>(
        test_api(*payment_link_manager_).GetApiClient());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  MockFacilitatedPaymentsClient client_;
  optimization_guide::MockOptimizationGuideDecider optimization_guide_decider_;
  // Order matters here because `payment_link_manager_` keeps a reference
  // to `client_` and `optimization_guide_decider_`.
  std::unique_ptr<PaymentLinkManager> payment_link_manager_;
  std::unique_ptr<PrefService> pref_service_;
  syncer::TestSyncService sync_service_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<autofill::TestStrikeDatabase> test_strike_database_;
};

// Verify that metrics are logged correctly when a supported payment link is
// detected.
TEST_F(PaymentLinkManagerTest, LogPaymentLinkDetected) {
  base::HistogramTester histogram_tester;
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.PaymentLinkDetected",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PaymentLinkDetected::kEntryName,
      {ukm::builders::FacilitatedPayments_PaymentLinkDetected::
           kPaymentLinkDetectedName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PaymentLinkDetected"), true);
}

// Ewallet payment prompt is shown.
TEST_F(PaymentLinkManagerTest, EwalletPaymentPromptShown) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
}

// Ewallet payment prompt is not shown if payment link is not supported by
// available eWallet accounts.
TEST_F(PaymentLinkManagerTest,
       UnsupportedPaymentLink_EwalletPaymentPromptNotShown) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL unsupported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      unsupported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
}

// Ewallet payment prompt is not shown if payment link is invalid.
TEST_F(PaymentLinkManagerTest,
       InvalidPaymentLink_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL invalidPaymentLink("invalid://payment");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      invalidPaymentLink, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kLinkIsInvalid,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kLinkIsInvalid,
      /*expected_bucket_count=*/0);
}

// Ewallet payment prompt is not shown if there is no linked account.
TEST_F(PaymentLinkManagerTest, NoEwalletAccount_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kNoSupportedEwallet,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kNoSupportedEwallet,
      /*expected_bucket_count=*/1);
}

// Ewallet payment prompt is not shown if in landscape mode.
TEST_F(PaymentLinkManagerTest, InLandscapeMode_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, IsInLandscapeMode)
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kLandscapeScreenOrientation,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kLandscapeScreenOrientation,
      /*expected_bucket_count=*/1);
}

// Ewallet payment prompt is not shown if payments data manager is not
// available.
TEST_F(PaymentLinkManagerTest,
       PaymentsDataManagerUnavailable_EwalletPaymentPromptNotShown) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  ON_CALL(client_, GetPaymentsDataManager)
      .WillByDefault(testing::Return(nullptr));

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
}

// Ewallet payment prompt is not shown if the user has opted out of the eWallet
// flow.
TEST_F(PaymentLinkManagerTest, UserOptedOut_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  // Turn off eWallet pref.
  autofill::prefs::SetFacilitatedPaymentsEwallet(pref_service_.get(), false);

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kUserOptedOut,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kUserOptedOut,
      /*expected_bucket_count=*/1);
}

// Ewallet payment prompt is not shown if in foldable devices.
TEST_F(PaymentLinkManagerTest, IsFoldable_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, IsFoldable).Times(1).WillOnce(testing::Return(true));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kFoldableDevice,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kFoldableDevice,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       ApiClientAvailable_ApiClientAvailabilityCheckLatencyLogged) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(GetApiClient(), IsAvailableSync).Times(1).WillOnce([&]() {
    FastForwardBy(base::Seconds(2));
    return true;
  });

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.IsApiAvailable.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       ApiClientNotAvailable_ApiClientAvailabilityCheckLatencyLogged) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(GetApiClient(), IsAvailableSync).Times(1).WillOnce([&]() {
    FastForwardBy(base::Seconds(2));
    return false;
  });

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.IsApiAvailable.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// If the facilitated payment API is not available, then the manager doesn't
// show the eWallet payment prompt.
TEST_F(PaymentLinkManagerTest,
       ApiClientNotAvailable_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(GetApiClient(), IsAvailableSync)
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kApiClientNotAvailable,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kApiClientNotAvailable,
      /*expected_bucket_count=*/1);
}

// If the user selects an eWallet account in the payment prompt, request for
// risk data is made, and progress screen is shown.
TEST_F(PaymentLinkManagerTest,
       EwalletPaymentPromptAccepted_LoadRiskDataTriggered_ProgressScreenShown) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  EXPECT_CALL(client_, LoadRiskData(testing::_));
  EXPECT_CALL(client_, ShowProgressScreen());

  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);
}

TEST_F(PaymentLinkManagerTest, DeviceIsBound) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  EXPECT_TRUE(test_api(*payment_link_manager_).is_device_bound());
}

TEST_F(PaymentLinkManagerTest, DeviceIsNotBound) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/false));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  EXPECT_FALSE(test_api(*payment_link_manager_).is_device_bound());
}

// If the risk data is empty, then the manager does not retrieve a client token
// from the facilitated payments API client.
TEST_F(PaymentLinkManagerTest,
       RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_)).Times(0);
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(*payment_link_manager_)
      .OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2),
                        /*risk_data=*/"");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.LoadRiskData.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.LoadRiskData.Failure.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// If the risk data is not empty, then the manager retrieves a client token from
// the facilitated payments API client.
TEST_F(PaymentLinkManagerTest, RiskDataNotEmpty_GetClientTokenCalled) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_));

  test_api(*payment_link_manager_)
      .OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2),
                        /*risk_data=*/"fake risk data");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.LoadRiskData.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.LoadRiskData.Success.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// If the client token is empty, an error screen will be shown.
TEST_F(PaymentLinkManagerTest,
       OnGetClientToken_ClientTokenEmpty_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(*payment_link_manager_)
      .OnGetClientToken(base::TimeTicks::Now() - base::Seconds(2),
                        std::vector<uint8_t>{});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kClientTokenNotAvailable,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kClientTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that GetClientToken related metrics are logged correctly.
TEST_F(PaymentLinkManagerTest, LogGetClientTokenResultAndLatency) {
  for (bool get_client_token_result : {true, false}) {
    base::HistogramTester histogram_tester;

    test_api(*payment_link_manager_)
        .OnGetClientToken(base::TimeTicks::Now() - base::Seconds(2),
                          get_client_token_result
                              ? std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'}
                              : std::vector<uint8_t>{});

    std::string result = get_client_token_result ? "Success" : "Failure";

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Ewallet.GetClientToken.", result,
                      ".Latency"}),
        /*sample=*/2000,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Ewallet.GetClientToken.", result,
                      ".Latency.ShopeePay"}),
        /*sample=*/2000,
        /*expected_bucket_count=*/1);
  }
}

// Test that SendInitiatePaymentRequest doesn't initiates payment when
// FacilitatedPaymentsNetworkInterface is not available.
TEST_F(
    PaymentLinkManagerTest,
    SendInitiatePaymentRequest_PaymentsNetworkInterfaceNotAvailable_InitiatePaymentNotTriggered) {
  EXPECT_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
      .Times(1)
      .WillOnce(testing::Return(nullptr));

  EXPECT_CALL(*payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(*payment_link_manager_).SendInitiatePaymentRequest();
}

// Test that LogInitiatePaymentAttempt is logged correctly.
TEST_F(PaymentLinkManagerTest, LogInitiatePaymentAttempt) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_));

  test_api(*payment_link_manager_).SendInitiatePaymentRequest();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Attempt.ShopeePay",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` call has failure
// result, purchase action is not invoked. Instead, an error message is shown.
TEST_F(PaymentLinkManagerTest,
       OnInitiatePaymentResponseReceived_FailureResponse_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kPermanentFailure,
          std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Failure.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kInitiatePaymentFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kInitiatePaymentFailed,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` has empty action
// token, purchase action is not invoked. Instead, an error message is shown.
TEST_F(PaymentLinkManagerTest,
       OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that if the core account is std::nullopt, purchase action is not
// invoked. Instead, an error message is shown.
TEST_F(PaymentLinkManagerTest,
       OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, GetCoreAccountInfo)
      .Times(1)
      .WillOnce(testing::Return(std::nullopt));

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

// Test that if the user is logged out, purchase action is not invoked. Instead,
// an error message is shown.
TEST_F(PaymentLinkManagerTest,
       OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  ON_CALL(client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CoreAccountInfo()));

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Test that the puchase action is invoked after receiving a success response
// from the `FacilitatedPaymentsNetworkInterface::InitiatePayment` call.
TEST_F(PaymentLinkManagerTest,
       OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePayment.Success.Latency.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePurchaseAction.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.InitiatePurchaseAction.Attempt.ShopeePay",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that eWalet payment prompt is shown for websites in the allowlist.
TEST_F(PaymentLinkManagerTest,
       TriggerPaymentLinkPushPayment_UrlInAllowlist_EwalletPaymentPromptShown) {
  GURL page_url("https://example.com/");
  payments_data_manager_.AddEwalletForTest(autofill::Ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/page_url, u"ewallet_name", u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
       u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
      /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  ON_CALL(
      optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(page_url),
          testing::Eq(optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());
}

// Test that Ewallet payment prompt is not shown for webpages not in the
// allowlist.
TEST_F(
    PaymentLinkManagerTest,
    TriggerPaymentLinkPushPayment_UrlNotInAllowlist_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  GURL page_url("https://example.com/");
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  ON_CALL(
      optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(page_url),
          testing::Eq(optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kNotInAllowlist,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kNotInAllowlist,
      /*expected_bucket_count=*/0);
}

// Test that Ewallet payment prompt is not shown if the allowlist is not
// yet available
TEST_F(
    PaymentLinkManagerTest,
    TriggerPaymentLinkPushPayment_AllowlistNotAvailable_ApiAvailabilityNotInvoked) {
  GURL page_url("https://example.com/");
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  ON_CALL(
      optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(page_url),
          testing::Eq(optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kUnknown));

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());
}

// Test that when the eWallet FOP selector is shown, the latency UMA and FOP
// selector shown UKM metrics are logged.
TEST_F(PaymentLinkManagerTest,
       FopSelectorShown_LatencyHistogramAndShownUkmLogged) {
  base::HistogramTester histogram_tester;
  autofill::Ewallet supported_ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
       u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
      /*is_fido_enrolled=*/true);
  payments_data_manager_.AddEwalletForTest(supported_ewallet);
  GURL page_url("https://example.com/");
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  // Simulate eWallet payment flow is triggered.
  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());
  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));
  // Simulate that the FOP selector was shown successfully.
  test_api(*payment_link_manager_)
      .ShowPaymentLinkPrompt({supported_ewallet}, {}, base::DoNothing());
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);

  // Verify that when the eWallet FOP selector is shown, latency histogram is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletOnly.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletOnly.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::kEntryName,
      {ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::kShownName,
       ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
           kSchemeName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Scheme"), 2);
  EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
}

class PaymentLinkManagerTestForUiScreens
    : public PaymentLinkManagerTest,
      public testing::WithParamInterface<UiState> {
 public:
  void SetUp() override {
    // Default state.
    EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kHidden);

    switch (ui_state()) {
      case UiState::kFopSelector: {
        const std::vector<autofill::Ewallet> ewallets = {
            autofill::test::CreateEwalletAccount(100L)};
        test_api(*payment_link_manager_)
            .ShowPaymentLinkPrompt(std::move(ewallets), {}, base::DoNothing());
        break;
      }
      case UiState::kProgressScreen:
        test_api(*payment_link_manager_).ShowProgressScreen();
        break;
      case UiState::kErrorScreen:
        test_api(*payment_link_manager_).ShowErrorScreen();
        break;
      case UiState::kHidden:
        NOTREACHED();
    }
  }

  UiState ui_state() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(PaymentLinkManagerTest,
                         PaymentLinkManagerTestForUiScreens,
                         testing::Values(UiState::kFopSelector,
                                         UiState::kProgressScreen,
                                         UiState::kErrorScreen));

// Test that when a new screen is shown, UI state reflects the current UI being
// shown.
TEST_P(PaymentLinkManagerTestForUiScreens, NewScreenShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);

  // Verify feature has updated the UI state.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), ui_state());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.UiScreenShown",
      /*sample=*/ui_state(),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.UiScreenShown.ShopeePay",
      /*sample=*/ui_state(),
      /*expected_bucket_count=*/1);
}

// Test that when a new screen could not be shown, UI state is updated.
TEST_P(PaymentLinkManagerTestForUiScreens, NewScreenCouldNotBeShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen could not be shown.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kHidden);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed, but it was not due to a user action,
// the feature updates the UI state.
TEST_P(PaymentLinkManagerTestForUiScreens, ScreenClosedNotByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed, but it was not due to a user action.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kHidden);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed by the user, the feature updates the
// UI state.
TEST_P(PaymentLinkManagerTestForUiScreens, ScreenClosedByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed by the user.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kHidden);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when Chrome fails to invoke purchase action, the error screen is
// shown.
TEST_F(PaymentLinkManagerTest,
       OnTransactionResult_CouldNotInvoke_ErrorScreenShown) {
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(*payment_link_manager_)
      .OnTransactionResult(base::TimeTicks::Now() - base::Seconds(2),
                           PurchaseActionResult::kCouldNotInvoke);
}

// Test that when Chrome is successful in invoking the purchase action, the UI
// screen is dismissed.
TEST_F(PaymentLinkManagerTest, OnTransactionResult_ResultOk_UiScreenDismissed) {
  // `DismissPrompt` is called once when the purchase action result is
  // received, and again when the test fixture destroys the `manager_`.
  EXPECT_CALL(client_, DismissPrompt).Times(2);

  test_api(*payment_link_manager_)
      .OnTransactionResult(base::TimeTicks::Now() - base::Seconds(2),
                           PurchaseActionResult::kResultOk);
}

// Test that when Chrome is successful in invoking the purchase action, the UI
// screen is dismissed.
TEST_F(PaymentLinkManagerTest,
       OnTransactionResult_ResultCanceled_UiScreenDismissed) {
  // `DismissPrompt` is called once when the purchase action result is
  // received, and again when the test fixture destroys the `manager_`.
  EXPECT_CALL(client_, DismissPrompt).Times(2);

  test_api(*payment_link_manager_)
      .OnTransactionResult(base::TimeTicks::Now() - base::Seconds(2),
                           PurchaseActionResult::kResultCanceled);
}

class PaymentLinkManagerOnTransactionResultLoggingTest
    : public testing::TestWithParam<std::tuple<PurchaseActionResult, bool>> {
 public:
  PaymentLinkManagerOnTransactionResultLoggingTest()
      : payment_link_manager_(std::make_unique<PaymentLinkManager>(
            &client_, /*api_client_creator=*/
            base::BindRepeating(
                &MockFacilitatedPaymentsApiClient::CreateApiClient),
            &optimization_guide_decider_)) {
    test_api(*payment_link_manager_)
        .set_scheme(PaymentLinkValidator::Scheme::kShopeePay);
  }
  ~PaymentLinkManagerOnTransactionResultLoggingTest() = default;

  PurchaseActionResult purchase_action_result() const {
    return std::get<0>(GetParam());
  }

  bool is_device_bound() const { return std::get<1>(GetParam()); }

  std::string GetPurchaseActionResultString() const {
    switch (purchase_action_result()) {
      case PurchaseActionResult::kResultOk:
        return "Succeeded";
      case PurchaseActionResult::kCouldNotInvoke:
        return "Failed";
      case PurchaseActionResult::kResultCanceled:
        return "Abandoned";
    }
  }

  std::string GetIsDeviceBoundString() const {
    return is_device_bound() ? "DeviceBound" : "DeviceNotBound";
  }

 protected:
  MockFacilitatedPaymentsClient client_;
  optimization_guide::MockOptimizationGuideDecider optimization_guide_decider_;
  // Order matters here because `payment_link_manager_` keeps a reference
  // to `client_` and `optimization_guide_decider_`.
  std::unique_ptr<PaymentLinkManager> payment_link_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    PaymentLinkManagerTest,
    PaymentLinkManagerOnTransactionResultLoggingTest,
    testing::Combine(testing::Values(PurchaseActionResult::kResultOk,
                                     PurchaseActionResult::kCouldNotInvoke,
                                     PurchaseActionResult::kResultCanceled),
                     testing::Bool()));

// Test that when records for LogInitiatePurchaseActionResultAndLatency is
// correct when device is bounded.
TEST_P(PaymentLinkManagerOnTransactionResultLoggingTest,
       LogInitiatePurchaseActionResultAndLatency) {
  base::HistogramTester histogram_tester;
  test_api(*payment_link_manager_)
      .set_is_device_bound(/*is_device_bound=*/is_device_bound());
  test_api(*payment_link_manager_)
      .OnTransactionResult(base::TimeTicks::Now() - base::Seconds(2),
                           purchase_action_result());

  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                    GetPurchaseActionResultString(), ".Latency.",
                    GetIsDeviceBoundString()}),
      /*sample=*/2000,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({"FacilitatedPayments.Ewallet.InitiatePurchaseAction.",
                    GetPurchaseActionResultString(), ".Latency.ShopeePay.",
                    GetIsDeviceBoundString()}),
      /*sample=*/2000,
      /*expected_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       OnEwalletAccountSelected_HistogramLogged_SingleBound) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supportedPaymentLink(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supportedPaymentLink, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.FopSelector.UserAction.SingleBoundEwallet",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       OnEwalletAccountSelected_HistogramLogged_SingleUnboundEwallet) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/false));
  GURL supportedPaymentLink(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supportedPaymentLink, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.FopSelector.UserAction.SingleUnboundEwallet",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       OnEwalletAccountSelected_HistogramLogged_MultipleEwallets) {
  base::HistogramTester histogram_tester;
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname1",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name1", u"account_display_name1",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/false));
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/101, u"nickname2",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name2", u"account_display_name2",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/false));
  GURL supportedPaymentLink(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supportedPaymentLink, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.FopSelector.UserAction.MultipleEwallets",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest, OnPaymentPromptResult_FopSelectorAccepted) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  EXPECT_CALL(client_, LoadRiskData(testing::_));
  EXPECT_CALL(client_, ShowProgressScreen());

  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::kEntryName,
      {ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::
           kResultName,
       ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
           kSchemeName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"), true);
  EXPECT_EQ(ukm_entries[0].metrics.at("Scheme"), 2);
}

TEST_F(PaymentLinkManagerTest, ScreenClosedByUser_FopSelectorRejected) {
  const std::vector<autofill::Ewallet> ewallets = {
      autofill::test::CreateEwalletAccount(100L)};
  test_api(*payment_link_manager_)
      .ShowPaymentLinkPrompt(std::move(ewallets), {}, base::DoNothing());
  // Simulate new screen was shown successfully.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed by the user.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedByUser);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::kEntryName,
      {ukm::builders::FacilitatedPayments_Ewallet_FopSelectorResult::
           kResultName,
       ukm::builders::FacilitatedPayments_Ewallet_FopSelectorShown::
           kSchemeName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"), false);
  EXPECT_EQ(ukm_entries[0].metrics.at("Scheme"), 2);
}

TEST_F(PaymentLinkManagerTest,
       ProgressScreenAutoDismissedAfterInvokingPurchaseAction) {
  // When purchase action is invoked, the progress screen would be showing.
  test_api(*payment_link_manager_).ShowProgressScreen();

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  // The progress screen is persisted for a short duration after invoking the
  // purchase action for a smooth transition to the platform screen.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(),
            UiState::kProgressScreen);

  FastForwardBy(base::Seconds(1));

  // The progress screen should be dismissed after a short delay.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kHidden);
}

TEST_F(PaymentLinkManagerTest,
       ErrorScreenNotAutoDismissedAfterInvokingPurchaseAction) {
  // When purchase action is invoked, the progress screen would be showing.
  test_api(*payment_link_manager_).ShowProgressScreen();

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->secure_payload_ = CreateSecurePayload();
  test_api(*payment_link_manager_)
      .OnInitiatePaymentResponseReceived(
          base::TimeTicks::Now() - base::Seconds(2),
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));

  // If the purchase action could not be invoked, the `PurchaseActionResult` is
  // returned immediately. The error screen is shown.
  test_api(*payment_link_manager_)
      .OnTransactionResult(base::TimeTicks::Now() - base::Seconds(2),
                           PurchaseActionResult::kCouldNotInvoke);
  FastForwardBy(base::Seconds(1));

  // The error screen shouldn't be auto-dismissed.
  EXPECT_EQ(test_api(*payment_link_manager_).ui_state(), UiState::kErrorScreen);
}

// Test that eWallet payment prompt is shown if strike limitation is not
// reached.
TEST_F(
    PaymentLinkManagerTest,
    TriggerPaymentLinkPushPayment_NotEnoughStrike_EwalletPaymentPromptShown) {
  GURL page_url("https://example.com/");
  payments_data_manager_.AddEwalletForTest(autofill::Ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/page_url, u"ewallet_name", u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
       u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
      /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  PaymentLinkSuggestionStrikeDatabase strike_database =
      PaymentLinkSuggestionStrikeDatabase(test_strike_database_.get());
  strike_database.AddStrikes(1);

  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());
}

// Test that Ewallet payment prompt is not shown if strike limitation is
// reached.
TEST_F(PaymentLinkManagerTest,
       TriggerPaymentLinkPushPayment_MaxStrike_EwalletPaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  GURL page_url("https://example.com/");
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  PaymentLinkSuggestionStrikeDatabase strike_database =
      PaymentLinkSuggestionStrikeDatabase(test_strike_database_.get());
  strike_database.AddStrikes(5);

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.PayflowExitedReason.ShopeePay",
      /*sample=*/EwalletFlowExitedReason::kMaxStrikes,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTest,
       OnPaymentPromptResult_FopSelectorAccepted_ClearsStrikes) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  PaymentLinkSuggestionStrikeDatabase strike_database =
      PaymentLinkSuggestionStrikeDatabase(test_strike_database_.get());
  strike_database.AddStrikes(2);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  EXPECT_EQ(0, strike_database.GetStrikes());
}

class PaymentLinkManagerTestForA2AFlow : public PaymentLinkManagerTest {
 public:
  PaymentLinkManagerTestForA2AFlow()
      : mock_device_delegate_(std::make_unique<MockDeviceDelegate>()),
        mock_facilitated_payments_app_info_list_(
            std::make_unique<MockFacilitatedPaymentsAppInfoList>()) {
    ON_CALL(client_, GetDeviceDelegate)
        .WillByDefault(testing::Return(mock_device_delegate_.get()));
    ON_CALL(*mock_device_delegate_, GetSupportedPaymentApps)
        .WillByDefault([&]() {
          return std::move(mock_facilitated_payments_app_info_list_);
        });
    ON_CALL(optimization_guide_decider_,
            CanApplyOptimization(
                testing::_,
                testing::Eq(optimization_guide::proto::A2A_MERCHANT_ALLOWLIST),
                testing::A<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(testing::Return(
            optimization_guide::OptimizationGuideDecision::kTrue));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockDeviceDelegate> mock_device_delegate_;
  std::unique_ptr<MockFacilitatedPaymentsAppInfoList>
      mock_facilitated_payments_app_info_list_;
};

// A2A payment prompt is not shown when the flag is off.
TEST_F(PaymentLinkManagerTestForA2AFlow, FlagOff_A2APaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndDisableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kFlagNotEnabled, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kFlagNotEnabled, /*expected_bucket_count=*/1);
}

// A2A payment prompt is not shown when there are no supported payment apps.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       NoSupportedPaymentApps_A2APaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");

  EXPECT_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .Times(2)
      .WillRepeatedly(testing::Return(0));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kNoSupportedPaymentApp, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kNoSupportedPaymentApp, /*expected_bucket_count=*/1);
}

// A2A payment prompt is shown and latency metrics are logged.
TEST_F(PaymentLinkManagerTestForA2AFlow, A2APaymentPromptShown) {
  base::HistogramTester histogram_tester;

  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  EXPECT_CALL(*mock_facilitated_payments_app_info_list_, Size())
      .Times(2)
      .WillRepeatedly(testing::Return(2));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));

  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink.PromptPay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink",
      0);
}

// Payment prompt is shown with A2A and eWallet options and latency metrics are
// logged.
TEST_F(PaymentLinkManagerTestForA2AFlow, PaymentPromptShown_A2AAndEwallet) {
  base::HistogramTester histogram_tester;

  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  autofill::Ewallet supported_ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
       u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
      /*is_fido_enrolled=*/true);
  payments_data_manager_.AddEwalletForTest(supported_ewallet);
  GURL supported_payment_link(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));

  EXPECT_CALL(
      client_,
      ShowPaymentLinkPrompt(
          testing::Eq(std::vector<autofill::Ewallet>{supported_ewallet}),
          testing::_, testing::_));

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));

  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelectorShown."
      "LatencyAfterDetectingPaymentLink.ShopeePay",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// A2A payment prompt is shown for websites in the allolwist.
TEST_F(PaymentLinkManagerTestForA2AFlow, UrlInAllowlist_A2APaymentPromptShown) {
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL page_url("https://www.example.com");
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  GURL sanitized_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay");
  ON_CALL(optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(page_url),
              testing::Eq(optimization_guide::proto::A2A_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());
}

// A2A payment prompt is not shown for websites not in the allolwist.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       UrlNotInAllowlist_A2APaymentPromptNotShown) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL page_url("https://www.example.com");
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  ON_CALL(optimization_guide_decider_,
          CanApplyOptimization(
              testing::Eq(page_url),
              testing::Eq(optimization_guide::proto::A2A_MERCHANT_ALLOWLIST),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_CALL(*mock_device_delegate_, GetSupportedPaymentApps(testing::_))
      .Times(0);
  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, page_url, ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kNotInAllowlist, /*expected_bucket_count=*/1);
}

// Test when A2A payment prompt is shown, set
// FacilitatedPaymentsA2ATriggeredOnce pref to `true` is invoked;
TEST_F(PaymentLinkManagerTestForA2AFlow,
       A2APaymentPromptShown_A2ATriggeredOncePrefSet) {
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  EXPECT_CALL(*mock_facilitated_payments_app_info_list_, Size())
      .Times(2)
      .WillRepeatedly(testing::Return(2));
  EXPECT_CALL(client_, ShowPaymentLinkPrompt);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  EXPECT_TRUE(pref_service_.get()->GetBoolean(
      autofill::prefs::kFacilitatedPaymentsA2ATriggeredOnce));
}

// A2A payment prompt is not shown if the user has opted out of the A2A flow.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       UserOptedOut_A2APaymentPromptNotShown) {
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));

  // Test that when `kFacilitatedPaymentsA2AEnabled` pref is true,
  // `ShowPaymentLinkPrompt` is invoked.
  pref_service_.get()->SetBoolean(
      autofill::prefs::kFacilitatedPaymentsA2AEnabled, true);

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(1);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Test that when `kFacilitatedPaymentsA2AEnabled` pref is false,
  // `ShowPaymentLinkPrompt` is not invoked.
  pref_service_.get()->SetBoolean(
      autofill::prefs::kFacilitatedPaymentsA2AEnabled, false);
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, ShowPaymentLinkPrompt).Times(0);

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kUserOptedOut, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kUserOptedOut, /*expected_bucket_count=*/1);
}

// Test that when a payment app is selected, the strikes are cleared.
TEST_F(PaymentLinkManagerTestForA2AFlow, OnPaymentAppSelected_ClearsStrikes) {
  // Setup strikes.
  PaymentLinkSuggestionStrikeDatabase strike_database =
      PaymentLinkSuggestionStrikeDatabase(test_strike_database_.get());
  strike_database.AddStrikes(2);
  ASSERT_EQ(2, strike_database.GetStrikes());

  test_api(*payment_link_manager_).set_is_payment_app_available(true);

  // Trigger the call.
  test_api(*payment_link_manager_)
      .OnPaymentAppSelected("com.example.app", "com.example.app.activity");

  // Verify strikes are cleared.
  EXPECT_EQ(0, strike_database.GetStrikes());
}

// Test that when a payment app is selected, the app is invoked.
TEST_F(PaymentLinkManagerTestForA2AFlow, OnPaymentAppSelected_InvokesApp) {
  // Setup for InvokePaymentApp call.
  const std::string package_name = "com.example.app";
  const std::string activity_name = "com.example.app.activity";
  const std::string payment_link =
      "https://www.itmx.co.th/facilitated-payment/prompt-pay";
  auto request_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  request_details->payment_link_ = payment_link;
  test_api(*payment_link_manager_)
      .set_initiate_payment_request_details(std::move(request_details));

  test_api(*payment_link_manager_).set_is_payment_app_available(true);

  EXPECT_CALL(
      *mock_device_delegate_,
      InvokePaymentApp(package_name, activity_name, GURL(payment_link)));
  // `DismissPrompt` is called once when a payment app is selected, and again
  // when the test fixture destroys the `manager_`.
  EXPECT_CALL(client_, DismissPrompt).Times(2);

  // Trigger the call.
  test_api(*payment_link_manager_)
      .OnPaymentAppSelected(package_name, activity_name);
}

// Test that when a payment app is selected, the app is invoked.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       OnPaymentAppSelected_InvokedAppSucceed_RecordHistogram) {
  base::HistogramTester histogram_tester;
  // Setup for InvokePaymentApp call.
  const std::string package_name = "com.example.app";
  const std::string activity_name = "com.example.app.activity";
  const std::string payment_link =
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path";
  auto request_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  request_details->payment_link_ = payment_link;
  test_api(*payment_link_manager_)
      .set_initiate_payment_request_details(std::move(request_details));

  test_api(*payment_link_manager_)
      .set_is_payment_app_available(/*is_payment_app_available=*/true);
  test_api(*payment_link_manager_)
      .set_scheme(PaymentLinkValidator::Scheme::kPromptPay);

  EXPECT_CALL(*mock_device_delegate_,
              InvokePaymentApp(package_name, activity_name, GURL(payment_link)))
      .WillOnce(testing::Return(true));

  // Trigger the call.
  test_api(*payment_link_manager_)
      .OnPaymentAppSelected(package_name, activity_name);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelector.UserAction",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelector.UserAction.PromptPay",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.A2A.InvokePaymentApp.Success."
      "LatencyAfterDetectingPaymentLink",
      1);
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.A2A.InvokePaymentApp.Success."
      "LatencyAfterDetectingPaymentLink.PromptPay",
      1);
}

// Test that when a payment app is selected, the app is invoked.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       OnPaymentAppSelected_InvokedAppFailed_RecordHistogram) {
  base::HistogramTester histogram_tester;
  // Setup for InvokePaymentApp call.
  const std::string package_name = "com.example.app";
  const std::string activity_name = "com.example.app.activity";
  const std::string payment_link =
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path";
  auto request_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  request_details->payment_link_ = payment_link;
  test_api(*payment_link_manager_)
      .set_initiate_payment_request_details(std::move(request_details));

  test_api(*payment_link_manager_)
      .set_is_payment_app_available(/*is_payment_app_available=*/true);
  test_api(*payment_link_manager_)
      .set_scheme(PaymentLinkValidator::Scheme::kPromptPay);

  EXPECT_CALL(*mock_device_delegate_,
              InvokePaymentApp(package_name, activity_name, GURL(payment_link)))
      .WillOnce(testing::Return(false));

  // Trigger the call.
  test_api(*payment_link_manager_)
      .OnPaymentAppSelected(package_name, activity_name);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelector.UserAction",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2AOnly.FopSelector.UserAction.PromptPay",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.A2A.InvokePaymentApp.Failure."
      "LatencyAfterDetectingPaymentLink",
      1);
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.A2A.InvokePaymentApp.Failure."
      "LatencyAfterDetectingPaymentLink.PromptPay",
      1);
}

TEST_F(PaymentLinkManagerTestForA2AFlow,
       OnPaymentAppSelected_WithEwalletAvailable_RecordHistogram) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);

  // Setup for InvokePaymentApp call.
  const std::string package_name = "com.example.app";
  const std::string activity_name = "com.example.app.activity";
  const GURL payment_link(
      "https://www.itmx.co.th/facilitated-payment/"
      "prompt-pay?path=fake_path");

  // Setup eWallet.
  payments_data_manager_.AddEwalletForTest(autofill::Ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^https:\\/\\/www\\.itmx\\.co\\.th\\/facilitated-payment\\/"
       u"prompt-pay.+"},
      /*is_fido_enrolled=*/true));

  // Setup payment app.
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(1));

  // Trigger payment flow.
  payment_link_manager_->TriggerPaymentLinkPushPayment(
      payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  FastForwardBy(base::Seconds(2));

  EXPECT_CALL(*mock_device_delegate_,
              InvokePaymentApp(package_name, activity_name, payment_link))
      .WillOnce(testing::Return(true));
  // Trigger the call.
  test_api(*payment_link_manager_)
      .OnPaymentAppSelected(package_name, activity_name);

  // Verify histograms.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelector.UserAction",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelector.UserAction."
      "PromptPay",
      PaymentLinkFopSelectorAction::kPaymentAppSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.InvokePaymentApp.Success."
      "LatencyAfterDetectingPaymentLink",
      2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.InvokePaymentApp.Success."
      "LatencyAfterDetectingPaymentLink.PromptPay",
      2000,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTestForA2AFlow, OnEwalletSelected_RecordHistogram) {
  base::HistogramTester histogram_tester;
  const GURL payment_link(
      "https://www.itmx.co.th/facilitated-payment/"
      "prompt-pay?path=fake_path");

  // Setup eWallet.
  payments_data_manager_.AddEwalletForTest(autofill::Ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^https:\\/\\/www\\.itmx\\.co\\.th\\/facilitated-payment\\/"
       u"prompt-pay.+"},
      /*is_fido_enrolled=*/true));

  // Trigger payment flow.
  payment_link_manager_->TriggerPaymentLinkPushPayment(
      payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Trigger the eWallet selection.
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  // Verify histograms.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletOnly.FopSelector.UserAction",
      PaymentLinkFopSelectorAction::kEwalletSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletOnly.FopSelector.UserAction.PromptPay",
      PaymentLinkFopSelectorAction::kEwalletSelected,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentLinkManagerTestForA2AFlow,
       OnEwalletSelected_WithPaymentAppAvailable_RecordHistogram) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);

  const GURL payment_link(
      "https://www.itmx.co.th/facilitated-payment/"
      "prompt-pay?path=fake_path");

  // Setup eWallet.
  payments_data_manager_.AddEwalletForTest(autofill::Ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^https:\\/\\/www\\.itmx\\.co\\.th\\/facilitated-payment\\/"
       u"prompt-pay.+"},
      /*is_fido_enrolled=*/true));

  // Setup payment app.
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(1));

  // Trigger payment flow.
  payment_link_manager_->TriggerPaymentLinkPushPayment(
      payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Trigger the eWallet selection.
  test_api(*payment_link_manager_)
      .OnEwalletAccountSelected(/*selected_instrument_id=*/100L);

  // Verify histograms.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelector.UserAction",
      PaymentLinkFopSelectorAction::kEwalletSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.EwalletAndA2A.FopSelector.UserAction."
      "PromptPay",
      PaymentLinkFopSelectorAction::kEwalletSelected,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kOtherFopSelected, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kOtherFopSelected, /*expected_bucket_count=*/1);
}

// Test that when FOP selector is closed not by user, A2A flow exited reason is
// logged.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       FopSelectorClosedNotByUser_A2AFlowExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Simulate FOP selector was shown.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate FOP selector was closed not by user.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedNotByUser);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/1);
}

// Test that when FOP selector is closed by user, A2A flow exited reason is
// logged.
TEST_F(PaymentLinkManagerTestForA2AFlow,
       FopSelectorClosedByUser_A2AFlowExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  feature_list_.InitAndEnableFeature(
      payments::facilitated::kFacilitatedPaymentsEnableA2APayment);
  GURL supported_payment_link(
      "https://www.itmx.co.th/facilitated-payment/prompt-pay?path=fake_path");
  ON_CALL(*mock_facilitated_payments_app_info_list_, Size)
      .WillByDefault(testing::Return(2));

  payment_link_manager_->TriggerPaymentLinkPushPayment(
      supported_payment_link, GURL("https://www.example.com"),
      ukm::UkmRecorder::GetNewSourceID());

  // Simulate FOP selector was shown.
  test_api(*payment_link_manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
  // Simulate FOP selector was closed by user.
  test_api(*payment_link_manager_)
      .OnUiScreenEvent(UiEvent::kScreenClosedByUser);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason",
      A2AFlowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.A2A.PayflowExitedReason.PromptPay",
      A2AFlowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/1);
}

}  // namespace payments::facilitated
