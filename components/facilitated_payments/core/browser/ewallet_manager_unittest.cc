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
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {

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
    ON_CALL(client_, IsInLandscapeMode).WillByDefault(testing::Return(false));
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

}  // namespace payments::facilitated
