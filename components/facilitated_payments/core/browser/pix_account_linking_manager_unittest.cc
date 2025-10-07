// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager_test_api.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments::facilitated {

class PixAccountLinkingManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  PixAccountLinkingManagerTest() {
    manager_ = std::make_unique<PixAccountLinkingManager>(&client_);
  }

  void SetUp() override {
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    payments_data_manager_->SetPaymentsCustomerData(
        std::make_unique<autofill::PaymentsCustomerData>("123456"));
    payments_data_manager_->SetAccountInfoForPayments(
        identity_test_env_.MakePrimaryAccountAvailable(
            "somebody@example.test", signin::ConsentLevel::kSignin));
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(payments_data_manager_.get()));
    device_delegate_ = std::make_unique<MockDeviceDelegate>();
    ON_CALL(client_, GetDeviceDelegate)
        .WillByDefault(testing::Return(device_delegate_.get()));
    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), *payments_data_manager_);
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));

    // Success path setup. The Pix account linking user pref is default enabled.
    ON_CALL(client_, GetLastCommittedOrigin)
        .WillByDefault(testing::ReturnRef(kPixPaymentPageOrigin));
    ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
        .WillByDefault(
            testing::Return(WalletEligibilityForPixAccountLinking::kEligible));
    ON_CALL(client(), IsWebContentsVisibleOrOccluded)
        .WillByDefault(testing::Return(true));
    // Simulate the payments server returns that the user is eligible for Pix
    // account linking.
    ON_CALL(*payments_network_interface(),
            GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                 testing::_))
        .WillByDefault([](long, auto callback, const std::string&) {
          std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kSuccess,
                                  true);
          return base::StrongAlias<autofill::payments::RequestIdTag,
                                   std::string>();
        });
    // Simulate user leaving and returning to Chrome, after which the callback
    // that triggers showing the prompt is called.
    ON_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
        .WillByDefault(
            [](base::OnceClosure callback) { std::move(callback).Run(); });
    ON_CALL(client_, HasScreenlockOrBiometricSetup)
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    payments_data_manager_->ClearAllServerDataForTesting();
    payments_data_manager_.reset();
  }

 protected:
  MockFacilitatedPaymentsClient& client() { return client_; }
  PixAccountLinkingManager* manager() { return manager_.get(); }
  MockDeviceDelegate* device_delegate() { return device_delegate_.get(); }
  inline PixAccountLinkingManagerTestApi test_api() {
    return PixAccountLinkingManagerTestApi(manager_.get());
  }
  MockFacilitatedPaymentsNetworkInterface* payments_network_interface() {
    return payments_network_interface_.get();
  }

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;
  const url::Origin kPixPaymentPageOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const base::TimeDelta kShowPromptDelay = base::Seconds(3);

 private:
  // Order matters here because `manager_` keeps a reference to `client_`.
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<PixAccountLinkingManager> manager_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockDeviceDelegate> device_delegate_;
};

TEST_F(PixAccountLinkingManagerTest, SuccessPathShowsPrompt) {
  // The prompt should not be shown synchronously.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  // Expect the prompt to be shown then.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  // Fast-forward time by 3 seconds to trigger the delayed task.
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingNotSupported_PromptNotShown) {
  ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(
          WalletEligibilityForPixAccountLinking::kWalletNotInstalled));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingPrefDisabled_PromptNotShown) {
  autofill::prefs::SetFacilitatedPaymentsPixAccountLinking(pref_service_.get(),
                                                           false);

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       NoPaymentsProfile_ServerEligibilityNotChecked_PromptShown) {
  payments_data_manager_->ClearPaymentsCustomerData();

  // Backend call for GetDetailsForPaymentInstrument should not be called if
  // user is not a payments customer. But, the prompt should still be shown.
  EXPECT_CALL(
      *payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckNotCompleted_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(
          base::StrongAlias<autofill::payments::RequestIdTag, std::string>()));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsIneligible_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce([](long, auto callback, const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                false);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, TabNotActive_PromptNotShown) {
  ON_CALL(client(), IsWebContentsVisibleOrOccluded)
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, UserNotReturnedToChrome_PromptNotShown) {
  // Simulate user not returning to Chrome, so the callback is never run.
  EXPECT_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
      .WillOnce([](base::OnceClosure callback) {});

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, DifferentOrigin_PromptNotShown) {
  // Simulate that when the user returns to Chrome, they are on a different
  // website.
  url::Origin different_website_origin =
      url::Origin::Create(GURL("https://www.different.com"));
  ON_CALL(client(), GetLastCommittedOrigin)
      .WillByDefault(testing::ReturnRef(different_website_origin));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, DismissPrompt) {
  // Verify that the prompt dismissal is triggered only once despite multiple
  // calls to `DismissPrompt`.
  EXPECT_CALL(client(), DismissPrompt);

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().DismissPrompt();
  // This call should not trigger prompt dismissal again.
  test_api().DismissPrompt();
}

TEST_F(PixAccountLinkingManagerTest, OnAccepted) {
  EXPECT_CALL(client(), DismissPrompt);
  EXPECT_CALL(*device_delegate(),
              LaunchPixAccountLinkingPage("somebody@example.test"));

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();
}

TEST_F(PixAccountLinkingManagerTest, AccountInfoNotValid_WalletNotLaunched) {
  // Set account info to empty.
  payments_data_manager_->SetAccountInfoForPayments(CoreAccountInfo());

  EXPECT_CALL(client(), DismissPrompt);
  EXPECT_CALL(*device_delegate(), LaunchPixAccountLinkingPage(testing::_))
      .Times(0);

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();
}

TEST_F(PixAccountLinkingManagerTest, PromptDeclined_UserPrefUpdated) {
  // The account linking user pref should be default enabled .
  ASSERT_TRUE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
      pref_service_.get()));

  EXPECT_CALL(client(), DismissPrompt);

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnDeclined();

  // Verify that declining the prompt disables the account linking user pref.
  EXPECT_FALSE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
      pref_service_.get()));
}

TEST_F(PixAccountLinkingManagerTest, Reset_PromptShowing_TriggersDismissal) {
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  EXPECT_CALL(client(), DismissPrompt());

  test_api().Reset();
}

TEST_F(PixAccountLinkingManagerTest,
       Reset_NoPromptShowing_DoesNotTriggerDismissal) {
  EXPECT_CALL(client(), DismissPrompt).Times(0);

  test_api().Reset();
}

// During the account linking flow, the only async calls are server call to get
// eligibility, and waiting for user to complete payment and return to Chrome.
// Since these happen in parallel, and the latter call happens last, it is
// sufficient to test the latter for invalidated weak pointer.
TEST_F(PixAccountLinkingManagerTest,
       Reset_BeforeReturningToChrome_PromptNotShown) {
  base::OnceClosure on_return_to_chrome_callback;
  // Override the default behavior of
  // SetOnReturnToChromeCallbackAndObserveAppState to capture the callback and
  // simulate an async response.
  ON_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
      .WillByDefault([&](base::OnceClosure callback) {
        on_return_to_chrome_callback = std::move(callback);
      });

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  // Reset() is called before the user returns to Chrome. This should invalidate
  // the weak pointer for the callback.
  test_api().Reset();
  // The user returns to Chrome.
  ASSERT_TRUE(on_return_to_chrome_callback);
  std::move(on_return_to_chrome_callback).Run();
}

TEST_F(PixAccountLinkingManagerTest, ScreenlockNotEnabled_PromptNotShown) {
  ON_CALL(client(), HasScreenlockOrBiometricSetup)
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
}

TEST_F(PixAccountLinkingManagerTest, PromptAcceptedLogged) {
  base::HistogramTester histogram_tester;

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.PromptAccepted",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, ScreenShown_PromptShownLogged) {
  base::HistogramTester histogram_tester;

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnUiScreenEvent(UiEvent::kNewScreenShown);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.PromptShown",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, ScreenNotShown_PromptShownNotLogged) {
  base::HistogramTester histogram_tester;

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnUiScreenEvent(UiEvent::kScreenCouldNotBeShown);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.PromptShown",
      /*sample=*/true,
      /*expected_bucket_count=*/0);
}

class PixAccountLinkingManagerParameterizedTest
    : public PixAccountLinkingManagerTest,
      public testing::WithParamInterface<bool> {};

TEST_P(PixAccountLinkingManagerParameterizedTest,
       GetDetailsForCreatePaymentInstrument_ResultAndLatencyLogged) {
  base::HistogramTester histogram_tester;

  test_api().OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      base::TimeTicks::Now() - base::Seconds(2),
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*is_eligible_for_pix_account_linking=*/GetParam());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/GetParam(),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(PixAccountLinkingManagerTestSuite,
                         PixAccountLinkingManagerParameterizedTest,
                         testing::Bool());

TEST_F(PixAccountLinkingManagerTest, PromptDeclined_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnDeclined();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kUserDeclined,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kScreenClosedNotByUser,
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kScreenClosedByUser,
      /*expected_count=*/0);
}

TEST_F(PixAccountLinkingManagerTest, WalletNotInstalled_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(
          WalletEligibilityForPixAccountLinking::kWalletNotInstalled));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kWalletNotInstalled,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       WalletVersionNotSupported_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(
          WalletEligibilityForPixAccountLinking::kWalletVersionNotSupported));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kWalletVersionNotSupported,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingPrefDisabled_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  autofill::prefs::SetFacilitatedPaymentsPixAccountLinking(pref_service_.get(),
                                                           false);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kUserOptedOut,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, ScreenlockNotEnabled_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  ON_CALL(client(), HasScreenlockOrBiometricSetup)
      .WillByDefault(testing::Return(false));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/
      PixAccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckNotCompleted_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(
          base::StrongAlias<autofill::payments::RequestIdTag, std::string>()));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kServerSideIneligible,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsIneligible_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce([](long, auto callback, const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                false);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kServerSideIneligible,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, TabNotActive_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  ON_CALL(client(), IsWebContentsVisibleOrOccluded)
      .WillByDefault(testing::Return(false));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kTabIsNotActive,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, DifferentOrigin_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  // Simulate that when the user returns to Chrome, they are on a different
  // website.
  url::Origin different_website_origin =
      url::Origin::Create(GURL("https://www.different.com"));
  ON_CALL(client(), GetLastCommittedOrigin)
      .WillByDefault(testing::ReturnRef(different_website_origin));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/PixAccountLinkingFlowExitedReason::kUserSwitchedWebsite,
      /*expected_bucket_count=*/1);
}

class PixAccountLinkingManagerTestForExitedReasons
    : public PixAccountLinkingManagerTest,
      public testing::WithParamInterface<
          std::tuple<UiEvent, PixAccountLinkingFlowExitedReason>> {
 public:
  UiEvent ui_event() const { return std::get<0>(GetParam()); }

  PixAccountLinkingFlowExitedReason pix_account_linking_flow_exited_reason()
      const {
    return std::get<1>(GetParam());
  }
};

TEST_P(PixAccountLinkingManagerTestForExitedReasons, FlowExitedReasonLogged) {
  base::HistogramTester histogram_tester;

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnUiScreenEvent(ui_event());

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/pix_account_linking_flow_exited_reason(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    PixAccountLinkingManagerTestSuite,
    PixAccountLinkingManagerTestForExitedReasons,
    testing::ValuesIn({
        std::make_tuple(UiEvent::kScreenCouldNotBeShown,
                        PixAccountLinkingFlowExitedReason::kScreenNotShown),
        std::make_tuple(
            UiEvent::kScreenClosedNotByUser,
            PixAccountLinkingFlowExitedReason::kScreenClosedNotByUser),
        std::make_tuple(UiEvent::kScreenClosedByUser,
                        PixAccountLinkingFlowExitedReason::kScreenClosedByUser),
    }));

}  // namespace payments::facilitated
