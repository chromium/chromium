// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_manager.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/ewallet_manager_test_api.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
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
  account.gaia = "foo-gaia-id";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  return account;
}

}  // namespace

class EwalletManagerTest : public testing::Test {
 public:
  EwalletManagerTest()
      : ewallet_manager_(
            &client_, /*api_client_creator=*/
            base::BindOnce(
                &MockFacilitatedPaymentsApiClient::CreateApiClient)) {
    // Using Autofill preferences since we use autofill's infra for syncing
    // eWallets.
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_.SetPrefService(pref_service_.get());
    payments_data_manager_.SetSyncServiceForTest(&sync_service_);
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(&payments_data_manager_));
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(&payments_network_interface_));
    ON_CALL(client_, IsInLandscapeMode).WillByDefault(testing::Return(false));
    ON_CALL(client_, GetCoreAccountInfo)
        .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

    // `initiate_payment_request_details_` is lazy initialized in the
    // implementation. Initialize it here so tests depending on it won't crash.
    test_api(ewallet_manager_)
        .set_initiate_payment_request_details(
            std::make_unique<
                FacilitatedPaymentsInitiatePaymentRequestDetails>());
  }

  MockFacilitatedPaymentsApiClient& GetApiClient() {
    return *static_cast<MockFacilitatedPaymentsApiClient*>(
        test_api(ewallet_manager_).GetApiClient());
  }

 protected:
  MockFacilitatedPaymentsClient client_;
  EwalletManager ewallet_manager_;
  std::unique_ptr<PrefService> pref_service_;
  syncer::TestSyncService sync_service_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  MockFacilitatedPaymentsNetworkInterface payments_network_interface_;
};

// The manager checks for API availability after payment link validation.
TEST_F(EwalletManagerTest, ApiClientCheckedForAvailability) {
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

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  ewallet_manager_.TriggerEwalletPushPayment(supportedPaymentLink,
                                             GURL("https://www.example.com"));
}

// API availability is not invoked if payment link is not supported by available
// eWallet accounts.
TEST_F(EwalletManagerTest,
       UnsupportedPaymentLink_ApiClientNotCheckedForAvailability) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL unsupportedPaymentLink(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  ewallet_manager_.TriggerEwalletPushPayment(unsupportedPaymentLink,
                                             GURL("https://www.example.com"));
}

// API availability is not invoked if payment link is invalid.
TEST_F(EwalletManagerTest,
       InvalidPaymentLink_ApiClientNotCheckedForAvailability) {
  payments_data_manager_.AddEwalletForTest(
      autofill::Ewallet(/*instrument_id=*/100, u"nickname",
                        /*display_icon_url=*/GURL("http://www.example.com"),
                        u"ewallet_name", u"account_display_name",
                        /*supported_payment_link_uris=*/
                        {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
                         u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
                        /*is_fido_enrolled=*/true));
  GURL invalidPaymentLink("invalid://payment");

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  ewallet_manager_.TriggerEwalletPushPayment(invalidPaymentLink,
                                             GURL("https://www.example.com"));
}

// API availability is not invoked if in landscape mode.
TEST_F(EwalletManagerTest, InLandscapeMode_ApiClientNotCheckedForAvailability) {
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

  EXPECT_CALL(client_, IsInLandscapeMode)
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  ewallet_manager_.TriggerEwalletPushPayment(supportedPaymentLink,
                                             GURL("https://www.example.com"));
}

// API availability is not invoked if payments data manager is not available.
TEST_F(EwalletManagerTest,
       PaymentsDataManagerUnavailable_ApiClientNotCheckedForAvailability) {
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

  EXPECT_CALL(client_, GetPaymentsDataManager)
      .Times(1)
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  ewallet_manager_.TriggerEwalletPushPayment(supportedPaymentLink,
                                             GURL("https://www.example.com"));
}

// If the facilitated payment API is available, then the manager shows the
// eWallet payment prompt.
TEST_F(EwalletManagerTest, ShowsEwalletPaymentPromptWhenApiClientAvailable) {
  autofill::Ewallet ewallet(
      /*instrument_id=*/100, u"nickname",
      /*display_icon_url=*/GURL("http://www.example.com"), u"ewallet_name",
      u"account_display_name",
      /*supported_payment_link_uris=*/
      {u"^shopeepay:\\/\\/shopeepay\\.com\\.my\\?code=.*$",
       u"^tngd:\\/\\/tngdigital\\.com\\.my\\?code=.*$"},
      /*is_fido_enrolled=*/true);
  payments_data_manager_.AddEwalletForTest(ewallet);
  GURL supportedPaymentLink(
      "shopeepay://shopeepay.com.my?code=https://shopeepay.com.my/"
      "281011051692389958586862838?merchant=Walmart&amount=101&currency=usd");

  ewallet_manager_.TriggerEwalletPushPayment(supportedPaymentLink,
                                             GURL("https://www.example.com"));

  EXPECT_CALL(client_,
              ShowEwalletPaymentPrompt(
                  testing::UnorderedElementsAreArray({ewallet}), testing::_));

  test_api(ewallet_manager_).OnApiAvailabilityReceived(true);
}

// If the facilitated payment API is not available, then the manager doesn't
// show the eWallet payment prompt.
TEST_F(EwalletManagerTest,
       NotShowEwalletPaymentPromptWhenApiClientNotAvailable) {
  EXPECT_CALL(client_, ShowEwalletPaymentPrompt).Times(0);

  test_api(ewallet_manager_).OnApiAvailabilityReceived(false);
}

// If the user does not select an eWallet account in the payment prompt, request
// for risk data is not made, and progress screen is not shown.
TEST_F(
    EwalletManagerTest,
    EwalletPaymentPromptNotAccepted_LoadRiskDataNotTriggered_ProgressScreenNotShown) {
  EXPECT_CALL(client_, LoadRiskData(testing::_)).Times(0);
  EXPECT_CALL(client_, ShowProgressScreen()).Times(0);

  test_api(ewallet_manager_)
      .OnEwalletPaymentPromptResult(/*is_prompt_accepted=*/false,
                                    /*selected_instrument_id=*/0);
}

// If the user selects an eWallet account in the payment prompt, request for
// risk data is made, and progress screen is shown.
TEST_F(EwalletManagerTest,
       EwalletPaymentPromptAccepted_LoadRiskDataTriggered_ProgressScreenShown) {
  EXPECT_CALL(client_, LoadRiskData(testing::_));
  EXPECT_CALL(client_, ShowProgressScreen());

  test_api(ewallet_manager_)
      .OnEwalletPaymentPromptResult(/*is_prompt_accepted=*/true,
                                    /*selected_instrument_id=*/100L);
}

// If the risk data is empty, then the manager does not retrieve a client token
// from the facilitated payments API client.
TEST_F(EwalletManagerTest,
       RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_)).Times(0);
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(ewallet_manager_).OnRiskDataLoaded(/*risk_data=*/"");
}

// If the risk data is not empty, then the manager retrieves a client token from
// the facilitated payments API client.
TEST_F(EwalletManagerTest, RiskDataNotEmpty_GetClientTokenCalled) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_));

  test_api(ewallet_manager_).OnRiskDataLoaded(/*risk_data=*/"fake risk data");
}

// If the client token is empty, an error screen will be shown.
TEST_F(EwalletManagerTest, OnGetClientToken_ClientTokenEmpty_ErrorScreenShown) {
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(ewallet_manager_).OnGetClientToken(std::vector<uint8_t>{});
}

// Test that SendInitiatePaymentRequest doesn't initiates payment when
// FacilitatedPaymentsNetworkInterface is not available.
TEST_F(
    EwalletManagerTest,
    SendInitiatePaymentRequest_PaymentsNetworkInterfaceNotAvailable_InitiatePaymentNotTriggered) {
  EXPECT_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
      .Times(1)
      .WillOnce(testing::Return(nullptr));

  EXPECT_CALL(payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(client_, ShowErrorScreen);

  test_api(ewallet_manager_).SendInitiatePaymentRequest();
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` call has failure
// result, purchase action is not invoked. Instead, an error message is shown.
TEST_F(EwalletManagerTest,
       OnInitiatePaymentResponseReceived_FailureResponse_ErrorScreenShown) {
  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  test_api(ewallet_manager_)
      .OnInitiatePaymentResponseReceived(
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kPermanentFailure,
          std::move(response_details));
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` has empty action
// token, purchase action is not invoked. Instead, an error message is shown.
TEST_F(EwalletManagerTest,
       OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown) {
  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  test_api(ewallet_manager_)
      .OnInitiatePaymentResponseReceived(
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));
}

// Test that if the core account is std::nullopt, purchase action is not
// invoked. Instead, an error message is shown.
TEST_F(EwalletManagerTest,
       OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown) {
  EXPECT_CALL(client_, GetCoreAccountInfo)
      .Times(1)
      .WillOnce(testing::Return(std::nullopt));

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  test_api(ewallet_manager_)
      .OnInitiatePaymentResponseReceived(
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));
}

// Test that if the user is logged out, purchase action is not invoked. Instead,
// an error message is shown.
TEST_F(EwalletManagerTest,
       OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown) {
  ON_CALL(client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CoreAccountInfo()));

  EXPECT_CALL(client_, ShowErrorScreen);
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  test_api(ewallet_manager_)
      .OnInitiatePaymentResponseReceived(
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));
}

// Test that the puchase action is invoked after receiving a success response
// from the `FacilitatedPaymentsNetworkInterface::InitiatePayment` call.
TEST_F(EwalletManagerTest,
       OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered) {
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  test_api(ewallet_manager_)
      .OnInitiatePaymentResponseReceived(
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
              kSuccess,
          std::move(response_details));
}

}  // namespace payments::facilitated
