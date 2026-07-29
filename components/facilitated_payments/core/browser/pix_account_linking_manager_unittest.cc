// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager_test_api.h"
#include "components/facilitated_payments/core/browser/strike_databases/pix_account_linking_strike_database.h"
#include "components/facilitated_payments/core/features/features.h"
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

  PixAccountLinkingManagerTest() = default;

  std::unique_ptr<FacilitatedPaymentsApiClient> CreateApiClient() {
    return std::move(api_client_);
  }

  void SetUp() override {
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    payments_data_manager_->SetPaymentsCustomerData(
        std::make_unique<autofill::PaymentsCustomerData>("123456"));
    CoreAccountInfo account_info =
        identity_test_env_.MakePrimaryAccountAvailable(
            "somebody@example.test", signin::ConsentLevel::kSignin);
    payments_data_manager_->SetAccountInfoForPayments(account_info);
    ON_CALL(client_, GetCoreAccountInfo)
        .WillByDefault(testing::Return(account_info));
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

    api_client_ = std::make_unique<MockFacilitatedPaymentsApiClient>();
    api_client_ptr_ = api_client_.get();
    ON_CALL(*api_client_ptr_, GetClientToken(testing::_))
        .WillByDefault(
            [](base::OnceCallback<void(std::vector<uint8_t>)> callback) {
              std::move(callback).Run(std::vector<uint8_t>{1, 2, 3});
            });

    manager_ = std::make_unique<PixAccountLinkingManager>(
        &client_,
        base::BindRepeating(&PixAccountLinkingManagerTest::CreateApiClient,
                            base::Unretained(this)));

    // Success path setup. The Pix account linking user pref is default enabled.
    ON_CALL(client_, GetLastCommittedOrigin)
        .WillByDefault(testing::ReturnRef(kPixPaymentPageOrigin));
    ON_CALL(client(), IsWebContentsVisibleOrOccluded)
        .WillByDefault(testing::Return(true));
    // Simulate the payments server returns that the user is eligible for Pix
    // account linking.
    ON_CALL(*payments_network_interface(),
            GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                 testing::_, testing::_))
        .WillByDefault([](long, const std::vector<uint8_t>&, auto callback,
                          const std::string&) {
          std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kSuccess,
                                  true, std::vector<uint8_t>{1, 2, 3});
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
    test_strike_database_ = std::make_unique<autofill::TestStrikeDatabase>();
    ON_CALL(client_, GetStrikeDatabase)
        .WillByDefault(testing::Return(test_strike_database_.get()));
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

  std::unique_ptr<MockFacilitatedPaymentsApiClient> api_client_;
  raw_ptr<MockFacilitatedPaymentsApiClient> api_client_ptr_ = nullptr;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;
  const url::Origin kPixPaymentPageOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const base::TimeDelta kShowPromptDelay = base::Seconds(3);
  std::unique_ptr<autofill::TestStrikeDatabase> test_strike_database_;

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
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(0, testing::_, testing::_));

  // Fast-forward time by 3 seconds to trigger the delayed task.
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       UserReturnsBeforeBackendResponse_PromptShownWhenResponseArrives) {
  base::OnceCallback<void(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult, bool,
      const std::vector<uint8_t>&)>
      pending_rpc_callback;

  // Intercept the backend RPC callback so it does not run immediately.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([&pending_rpc_callback](long, const std::vector<uint8_t>&,
                                        auto callback, const std::string&) {
        pending_rpc_callback = std::move(callback);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  // User copies key and returns to Chrome.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  // Fast-forward delay time: RPC response has not arrived yet, prompt must NOT
  // show.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);
  task_environment_.FastForwardBy(kShowPromptDelay);

  // Now backend response completes with success (and action token {1, 2, 3}).
  // The prompt should be shown immediately because the 3s delay has already
  // passed.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(0, testing::_, testing::_));
  std::move(pending_rpc_callback)
      .Run(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
               kSuccess,
           true, std::vector<uint8_t>{1, 2, 3});
}

TEST_F(PixAccountLinkingManagerTest,
       BackendResponseBeforeUserReturn_PromptShownWhenUserReturns) {
  base::OnceClosure return_to_chrome_callback;

  // Intercept the return-to-chrome callback so it does not run immediately.
  EXPECT_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
      .WillOnce([&return_to_chrome_callback](base::OnceClosure callback) {
        return_to_chrome_callback = std::move(callback);
      });

  // User copies key on merchant page.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  // Prompt must NOT show before user returns.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);
  task_environment_.FastForwardBy(kShowPromptDelay);

  // User now returns to Chrome tab.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(0, testing::_, testing::_));
  std::move(return_to_chrome_callback).Run();

  // Fast-forward delay time.
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, CustomDelayShowsPrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnablePixAccountLinkingNative, {{"trigger_delay_seconds", "7"}});

  // The prompt should not be shown synchronously.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  // Fast-forward time by 3 seconds (default delay). The prompt should NOT be
  // shown yet.
  task_environment_.FastForwardBy(base::Seconds(3));

  // Expect the prompt to be shown then.
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(0, testing::_, testing::_));

  // Fast-forward time by another 4 seconds to reach 7 seconds.
  task_environment_.FastForwardBy(base::Seconds(4));
}

TEST_F(PixAccountLinkingManagerTest, ClientTokenNotAvailable_PromptNotShown) {
  ON_CALL(*api_client_ptr_, GetClientToken(testing::_))
      .WillByDefault(
          [](base::OnceCallback<void(std::vector<uint8_t>)> callback) {
            std::move(callback).Run(std::vector<uint8_t>{});
          });

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
       NoPaymentsProfile_ServerEligibilityChecked_PromptShown) {
  payments_data_manager_->ClearPaymentsCustomerData();

  // In native orchestration, GetDetailsForCreatePaymentInstrument is called
  // even if billing_customer_id == 0.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(0, testing::_, testing::_,
                                                   testing::_))
      .WillOnce([](long, const std::vector<uint8_t>&, auto callback,
                   const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, std::vector<uint8_t>{});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(0, testing::_, testing::_));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckNotCompleted_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce(testing::Return(
          base::StrongAlias<autofill::payments::RequestIdTag, std::string>()));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsIneligible_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([](long, const std::vector<uint8_t>&, auto callback,
                   const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                false, std::vector<uint8_t>{});
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
  EXPECT_CALL(*api_client_ptr_,
              InvokeInstrumentManager(testing::_, std::vector<uint8_t>{1, 2, 3},
                                      testing::_));

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();
}

TEST_F(PixAccountLinkingManagerTest,
       UserLoggedOut_InstrumentManagerNotInvoked) {
  // Set account info to empty.
  payments_data_manager_->SetAccountInfoForPayments(CoreAccountInfo());
  EXPECT_CALL(client(), GetCoreAccountInfo)
      .WillRepeatedly(testing::Return(std::nullopt));

  EXPECT_CALL(client(), DismissPrompt);
  EXPECT_CALL(*api_client_ptr_, InvokeInstrumentManager).Times(0);

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();
}

TEST_F(PixAccountLinkingManagerTest,
       PromptAccepted_ActionTokenNotAvailable_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  // Override server RPC to return empty action token.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([](long, const std::vector<uint8_t>&, auto callback,
                   const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                /*is_eligible=*/true, std::vector<uint8_t>{});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  EXPECT_CALL(*api_client_ptr_, InvokeInstrumentManager).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnAccepted();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       PromptDeclined_StrikeAdded_PrefNotDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  // The account linking user pref should be default enabled.
  ASSERT_TRUE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
      pref_service_.get()));

  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  ASSERT_EQ(strike_database.GetStrikes(), 0);

  EXPECT_CALL(client(), DismissPrompt);

  // The show method is called so the internal UI state is correctly set.
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
  test_api().OnDeclined();

  // Verify that declining the prompt adds a strike.
  EXPECT_EQ(strike_database.GetStrikes(), 1);

  // Verify that declining the prompt DOES NOT disable the account linking user
  // pref.
  EXPECT_TRUE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
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
       GetDetailsForCreatePaymentInstrumentResponse_UpdatesEligibility) {
  test_api().DoOnGetDetailsForCreatePaymentInstrumentResponse(
      /*is_eligible=*/GetParam());
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
      /*sample=*/AccountLinkingFlowExitedReason::kUserDeclined,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kScreenClosedNotByUser,
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kScreenClosedByUser,
      /*expected_count=*/0);
}

TEST_F(PixAccountLinkingManagerTest,
       ClientTokenNotAvailable_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
      .WillOnce(testing::Return());
  EXPECT_CALL(*api_client_ptr_, GetClientToken(testing::_))
      .WillOnce([](base::OnceCallback<void(std::vector<uint8_t>)> callback) {
        std::move(callback).Run(std::vector<uint8_t>{});
      });

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kClientTokenNotAvailable,
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
      /*sample=*/AccountLinkingFlowExitedReason::kUserOptedOut,
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
      AccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckNotCompleted_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult, bool,
      const std::vector<uint8_t>&)>
      pending_rpc_callback;

  // Simulate that the payments server response is in-flight.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([&pending_rpc_callback](long, const std::vector<uint8_t>&,
                                        auto callback, const std::string&) {
        pending_rpc_callback = std::move(callback);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  // While server response is in-flight, no exit reason should be logged yet.
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason", 0);

  // Server response completes with ineligible.
  std::move(pending_rpc_callback)
      .Run(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
               kSuccess,
           false, std::vector<uint8_t>{});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kNotEligiblePerPaymentsBackend,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsIneligible_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*device_delegate(), SetOnReturnToChromeCallbackAndObserveAppState)
      .WillOnce(testing::Return());
  // Simulate that the payments server returned ineligible.
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([](long, const std::vector<uint8_t>&, auto callback,
                   const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                false, std::vector<uint8_t>{});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kNotEligiblePerPaymentsBackend,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsEligible_PromptShown) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*payments_network_interface(),
              GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                   testing::_, testing::_))
      .WillOnce([](long, const std::vector<uint8_t>&, auto callback,
                   const std::string&) {
        std::move(callback).Run(
            autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
                kSuccess,
            /*is_eligible=*/true,
            /*action_token=*/
            std::vector<uint8_t>{'a', 'c', 't', 'i', 'o', 'n'});
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking."
      "GetDetailsForCreatePaymentInstrument.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, TabNotActive_ExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  ON_CALL(client(), IsWebContentsVisibleOrOccluded)
      .WillByDefault(testing::Return(false));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kTabIsNotActive,
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
  task_environment_.FastForwardBy(kShowPromptDelay);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kUserSwitchedWebsite,
      /*expected_bucket_count=*/1);
}

class PixAccountLinkingManagerTestForExitedReasons
    : public PixAccountLinkingManagerTest,
      public testing::WithParamInterface<
          std::tuple<UiEvent, AccountLinkingFlowExitedReason>> {
 public:
  UiEvent ui_event() const { return std::get<0>(GetParam()); }

  AccountLinkingFlowExitedReason pix_account_linking_flow_exited_reason()
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
                        AccountLinkingFlowExitedReason::kScreenNotShown),
        std::make_tuple(UiEvent::kScreenClosedNotByUser,
                        AccountLinkingFlowExitedReason::kScreenClosedNotByUser),
        std::make_tuple(UiEvent::kScreenClosedByUser,
                        AccountLinkingFlowExitedReason::kScreenClosedByUser),
    }));

TEST_F(PixAccountLinkingManagerTest,
       TriggerPixAccountLinking_MaxStrike_PromptNotShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  base::HistogramTester histogram_tester;
  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  strike_database.AddStrike();
  strike_database.AddStrike();
  strike_database.AddStrike();

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kMaxStrikes,
      /*expected_bucket_count=*/1);
}

TEST_F(
    PixAccountLinkingManagerTest,
    TriggerPixAccountLinking_RequiredDelayNotPassed_JustBeforeLimit_PromptNotShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  base::HistogramTester histogram_tester;
  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  strike_database.AddStrike();

  // Fast-forward time to just before 7 days (e.g., 6 days and 23 hours).
  task_environment_.FastForwardBy(base::Days(6) + base::Hours(23));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kRequiredDelayNotPassed,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       TriggerPixAccountLinking_NotEnoughStrike_PromptShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  strike_database.AddStrike();
  strike_database.AddStrike();

  // Fast-forward time by 7 days to pass the required delay.
  task_environment_.FastForwardBy(base::Days(7));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt(2, testing::_, testing::_));

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);
}

TEST_F(PixAccountLinkingManagerTest, PromptDismissedByUser_StrikeNotAdded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  ASSERT_EQ(strike_database.GetStrikes(), 0);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  // Simulate user dismissing the prompt (swiping away).
  test_api().OnUiScreenEvent(UiEvent::kScreenClosedByUser);

  EXPECT_EQ(strike_database.GetStrikes(), 0);
}

TEST_F(PixAccountLinkingManagerTest, PromptAccepted_StrikesCleared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnablePixAccountLinkingNative);

  PixAccountLinkingStrikeDatabase strike_database(test_strike_database_.get());
  strike_database.AddStrike();
  strike_database.AddStrike();
  ASSERT_EQ(strike_database.GetStrikes(), 2);

  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  test_api().OnAccepted();

  EXPECT_EQ(strike_database.GetStrikes(), 0);
}

TEST_F(PixAccountLinkingManagerTest, GetHistogramSuffix) {
  EXPECT_EQ(test_api().GetHistogramSuffix(), "Pix");
}

TEST_F(PixAccountLinkingManagerTest,
       GetPayloadForGetDetailsForCreatePaymentInstrument) {
  EXPECT_TRUE(
      test_api().GetPayloadForGetDetailsForCreatePaymentInstrument().empty());
}

TEST_F(PixAccountLinkingManagerTest, DoOnClientTokenReceived) {
  std::vector<uint8_t> expected_token = {'t', 'o', 'k', 'e', 'n'};

  test_api().DoOnClientTokenReceived(expected_token);

  EXPECT_EQ(test_api().client_token(), expected_token);
}

TEST_F(PixAccountLinkingManagerTest, DoOnAccountLinkingResult_Success) {
  base::HistogramTester histogram_tester;
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  EXPECT_CALL(client(), DismissPrompt());
  EXPECT_CALL(client(), ShowPixAccountLinkingSuccessScreen());

  test_api().DoOnAccountLinkingResult(AccountLinkingResult{
      /*is_successful=*/true, 12345L, AccountLinkingResultCode::kResultOk});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest,
       DoOnAccountLinkingResult_MissingInstrumentId) {
  base::HistogramTester histogram_tester;
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  EXPECT_CALL(client(), DismissPrompt());
  EXPECT_CALL(client(), ShowPixAccountLinkingSuccessScreen()).Times(0);
  EXPECT_CALL(client(), ShowAccountLinkingFailureNotification(
                            FacilitatedPaymentsType::kPix));

  test_api().DoOnAccountLinkingResult(
      AccountLinkingResult{/*is_successful=*/true, /*instrument_id=*/0,
                           AccountLinkingResultCode::kResultOk});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kGmsCoreFlowFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, DoOnAccountLinkingResult_Canceled) {
  base::HistogramTester histogram_tester;
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  EXPECT_CALL(client(), DismissPrompt());

  // Simulate user accepting prompt to launch GMSCore.
  test_api().OnAccepted();

  test_api().DoOnAccountLinkingResult(AccountLinkingResult{
      /*is_successful=*/false, 0, AccountLinkingResultCode::kResultCanceled});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kUserCanceledInGmsCore,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, DoOnAccountLinkingResult_Failure) {
  base::HistogramTester histogram_tester;
  manager()->MaybeShowPixAccountLinkingPrompt(kPixPaymentPageOrigin);
  task_environment_.FastForwardBy(kShowPromptDelay);

  EXPECT_CALL(client(), DismissPrompt());
  EXPECT_CALL(client(), ShowAccountLinkingFailureNotification(
                            FacilitatedPaymentsType::kPix));

  test_api().DoOnAccountLinkingResult(AccountLinkingResult{
      /*is_successful=*/false, 0, AccountLinkingResultCode::kResultError});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.AccountLinking.FlowExitedReason",
      /*sample=*/AccountLinkingFlowExitedReason::kGmsCoreFlowFailed,
      /*expected_bucket_count=*/1);
}

TEST_F(PixAccountLinkingManagerTest, CreateAccountLinkingParams) {
  EXPECT_FALSE(test_api().CreateAccountLinkingParams().has_value());
}

}  // namespace payments::facilitated
