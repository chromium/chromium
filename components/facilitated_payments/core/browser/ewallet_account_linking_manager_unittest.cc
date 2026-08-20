// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager_test_api.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using ::testing::_;

class EwalletAccountLinkingManagerTest : public testing::Test {
 public:
  EwalletAccountLinkingManagerTest() {
    manager_ = std::make_unique<EwalletAccountLinkingManager>(
        &client_, /*api_client_creator=*/
        base::BindRepeating(
            []() -> std::unique_ptr<FacilitatedPaymentsApiClient> {
              return std::make_unique<MockFacilitatedPaymentsApiClient>();
            }),
        autofill::Ewallet(/*instrument_id=*/0, /*nickname=*/u"",
                          /*display_icon_url=*/GURL(),
                          /*ewallet_name=*/u"eWallet",
                          /*account_display_name=*/u"",
                          /*supported_payment_link_uris=*/{},
                          /*is_fido_enrolled=*/false));

    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), payments_data_manager_);
  }

  void SetUp() override {
    payments_data_manager_.SetAutofillPaymentMethodsEnabled(true);
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(&payments_data_manager_));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockFacilitatedPaymentsClient client_;
  signin::IdentityTestEnvironment identity_test_env_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;
  std::unique_ptr<EwalletAccountLinkingManager> manager_;
};

TEST_F(EwalletAccountLinkingManagerTest, GetHistogramSuffix) {
  EXPECT_EQ("Ewallet", test_api(*manager_).GetHistogramSuffix());
}

TEST_F(EwalletAccountLinkingManagerTest,
       GetPayloadForGetDetailsForCreatePaymentInstrument) {
  base::DictValue expected_payload;
  EXPECT_EQ(
      expected_payload,
      test_api(*manager_).GetPayloadForGetDetailsForCreatePaymentInstrument());
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnAccountLinkingResult_InvokesCallback) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_cb;

  AccountLinkingResult expected_result{false, 0,
                                       AccountLinkingResultCode::kResultError};
  EXPECT_CALL(result_cb, Run(expected_result));

  manager_->TriggerAccountLinking(result_cb.Get());

  test_api(*manager_).DoOnAccountLinkingResult(expected_result);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnAccountLinkingResult_Canceled) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      false, 0, AccountLinkingResultCode::kResultCanceled});
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnAccountLinkingResult_Success) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnAccountLinkingResult(
      AccountLinkingResult{true, 12345, AccountLinkingResultCode::kResultOk});
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnAccountLinkingResult_CouldNotInvoke) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      false, 0, AccountLinkingResultCode::kCouldNotInvoke});
  // Verify that early exits do not log to the metric.
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.Result", 0);
}

TEST_F(EwalletAccountLinkingManagerTest, DismissAndCancelClearsState) {
  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_cb;

  EXPECT_CALL(result_cb, Run(_)).Times(0);

  manager_->TriggerAccountLinking(result_cb.Get());

  manager_->DismissAndCancel();

  // Since it was cancelled and the callback was reset, returning a result now
  // should not invoke the callback.
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      true, /*instrument_id=*/12345, AccountLinkingResultCode::kResultOk});
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnClientTokenReceived) {
  std::vector<uint8_t> client_token = {'t', 'o', 'k', 'e', 'n'};

  EXPECT_CALL(
      *payments_network_interface_,
      GetDetailsForCreatePaymentInstrument(0, client_token, _, "en-US"));

  test_api(*manager_).DoOnClientTokenReceived(client_token);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse) {
  base::OnceCallback<void()> on_accepted;
  base::OnceCallback<void()> on_declined;
  base::OnceCallback<void()> on_dismissed;

  EXPECT_CALL(client_,
              ShowAccountLinkingPrompt(
                  testing::AllOf(
                      testing::Field(&AccountLinkingParams::fop_type,
                                     FacilitatedPaymentsType::kEwallet),
                      testing::Field(&AccountLinkingParams::fop_display_name,
                                     u"eWallet"),
                      testing::Field(&AccountLinkingParams::strike_count, 0)),
                  _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_accepted = std::move(accepted);
        on_declined = std::move(declined);
        on_dismissed = std::move(dismissed);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  // Verify that the callbacks are successfully bound to the manager.
  ASSERT_TRUE(on_accepted);
  ASSERT_TRUE(on_declined);
  ASSERT_TRUE(on_dismissed);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Declined) {
  base::OnceCallback<void()> on_declined;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_declined = std::move(declined);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_declined);

  EXPECT_CALL(client_, DismissPrompt());
  std::move(on_declined).Run();
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Accepted) {
  base::OnceCallback<void()> on_accepted;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_accepted = std::move(accepted);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_accepted);

  // Since action_token_ is empty, it should exit with AccountLinkingResult{}
  // which has kCouldNotInvoke.
  EXPECT_CALL(client_, DismissPrompt());
  std::move(on_accepted).Run();
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Dismissed) {
  base::OnceCallback<void()> on_dismissed;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_dismissed = std::move(dismissed);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_dismissed);

  EXPECT_CALL(client_, DismissPrompt());
  std::move(on_dismissed).Run();
}

TEST_F(EwalletAccountLinkingManagerTest, CreateAccountLinkingParams) {
  auto params = test_api(*manager_).CreateAccountLinkingParams();
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->fop_type, FacilitatedPaymentsType::kEwallet);
  EXPECT_EQ(params->fop_display_name, u"eWallet");
  EXPECT_EQ(params->strike_count, 0);
}

TEST_F(EwalletAccountLinkingManagerTest, DismissAndCancel_InvalidatesWeakPtrs) {
  base::WeakPtr<NativeAccountLinkingHandler> weak_ptr =
      test_api(*manager_).GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
  manager_->DismissAndCancel();
  EXPECT_FALSE(weak_ptr);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DismissAndCancel_DismissesPromptIfShowing) {
  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _));
  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  EXPECT_CALL(client_, DismissPrompt());
  manager_->DismissAndCancel();
}

}  // namespace
}  // namespace payments::facilitated
