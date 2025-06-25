// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/mock_device_delegate.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager_test_api.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class PixAccountLinkingManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  PixAccountLinkingManagerTest() {
    manager_ = std::make_unique<PixAccountLinkingManager>(&client_);
    multiple_request_payments_network_interface_ = std::make_unique<
        MockMultipleRequestFacilitatedPaymentsNetworkInterface>(
        *identity_test_env_.identity_manager(), *payments_data_manager_);
  }

  void SetUp() override {
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    payments_data_manager_->SetPaymentsCustomerData(
        std::make_unique<autofill::PaymentsCustomerData>("123456"));
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(payments_data_manager_.get()));
    device_delegate_ = std::make_unique<MockDeviceDelegate>();
    ON_CALL(client_, GetDeviceDelegate)
        .WillByDefault(testing::Return(device_delegate_.get()));
    ON_CALL(client_, GetMultipleRequestFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(
            multiple_request_payments_network_interface_.get()));

    // Success path setup. The Pix account linking user pref is default enabled.
    ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
        .WillByDefault(testing::Return(true));
    // Simulate the payments server returns that the user is eligible for Pix
    // account linking.
    ON_CALL(*multiple_request_payments_network_interface(),
            GetDetailsForCreatePaymentInstrument(testing::_, testing::_,
                                                 testing::_))
        .WillByDefault(testing::Invoke([](long, auto callback,
                                          const std::string&) {
          std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kSuccess,
                                  true);
          return base::StrongAlias<autofill::payments::RequestIdTag,
                                   std::string>();
        }));
    // Simulate user leaving and returning to Chrome, after which the callback
    // that triggers showing the prompt is called.
    ON_CALL(*device_delegate(), SetOnReturnToChromeCallback)
        .WillByDefault(
            [](base::OnceClosure callback) { std::move(callback).Run(); });
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
  MockMultipleRequestFacilitatedPaymentsNetworkInterface*
  multiple_request_payments_network_interface() {
    return multiple_request_payments_network_interface_.get();
  }

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;

 private:
  // Order matters here because `manager_` keeps a reference to `client_`.
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<PixAccountLinkingManager> manager_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<MockMultipleRequestFacilitatedPaymentsNetworkInterface>
      multiple_request_payments_network_interface_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockDeviceDelegate> device_delegate_;
};

TEST_F(PixAccountLinkingManagerTest, SuccessPathShowsPrompt) {
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingNotSupported_PromptNotShown) {
  ON_CALL(*device_delegate(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingPrefDisabled_PromptNotShown) {
  autofill::prefs::SetFacilitatedPaymentsPixAccountLinking(pref_service_.get(),
                                                           false);

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       NoPaymentsProfile_ServerEligibilityNotChecked_PromptShown) {
  payments_data_manager_->ClearPaymentsCustomerData();

  // Backend call for GetDetailsForPaymentInstrument should not be called if
  // user is not a payments customer. But, the prompt should still be shown.
  EXPECT_CALL(
      *multiple_request_payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckNotCompleted_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *multiple_request_payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(
          base::StrongAlias<autofill::payments::RequestIdTag, std::string>()));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       ServerEligibilityCheckReturnsIneligible_PromptNotShown) {
  // Simulate that the payments server hasn't yet returned eligibility.
  EXPECT_CALL(
      *multiple_request_payments_network_interface(),
      GetDetailsForCreatePaymentInstrument(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](long, auto callback, const std::string&) {
        std::move(callback).Run(autofill::payments::PaymentsAutofillClient::
                                    PaymentsRpcResult::kSuccess,
                                false);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      }));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest, UserNotReturnedToChrome_PromptNotShown) {
  // Simulate user not returning to Chrome, so the callback is never run.
  EXPECT_CALL(*device_delegate(), SetOnReturnToChromeCallback)
      .WillOnce([](base::OnceClosure callback) {});

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest, OnAccepted) {
  EXPECT_CALL(client(), DismissPrompt);
  EXPECT_CALL(*device_delegate(), LaunchPixAccountLinkingPage);

  test_api().OnAccepted();
}

TEST_F(PixAccountLinkingManagerTest, PromptDeclined_UserPrefUpdated) {
  // The account linking user pref should be default enabled .
  ASSERT_TRUE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
      pref_service_.get()));

  EXPECT_CALL(client(), DismissPrompt);

  test_api().OnDeclined();

  // Verify that declining the prompt disables the account linking user pref.
  EXPECT_FALSE(autofill::prefs::IsFacilitatedPaymentsPixAccountLinkingEnabled(
      pref_service_.get()));
}

}  // namespace payments::facilitated
