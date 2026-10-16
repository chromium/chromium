// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::Field;

constexpr char kOfferUrl[] = "http://www.example.com/";
constexpr char kOtherUrl[] = "http://www.other.com/";

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(void,
              UpdateOfferNotification,
              (const AutofillOfferData&, const OfferNotificationOptions&),
              (override));
  MOCK_METHOD(void, DismissOfferNotification, (), (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    set_payments_autofill_client(
        std::make_unique<testing::NiceMock<MockPaymentsAutofillClient>>(this));
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *GetPaymentsAutofillClient());
  }
};

auto ShowsAutomatically(bool show_automatically) {
  return Field(&OfferNotificationOptions::show_notification_automatically,
               show_automatically);
}

class OfferNotificationHandlerTest : public testing::Test {
 public:
  OfferNotificationHandlerTest() {
    feature_list_.InitWithFeatures(
        {features::kAutofillEnableWalletDirectOffers,
         features::kAutofillEnableWalletDirectOffersNotificationBubble},
        {});
    client_.GetPersonalDataManager().SetSyncServiceForTest(&sync_service_);
  }

  void AddOffer() {
    client_.GetPersonalDataManager()
        .test_payments_data_manager()
        .AddAutofillOfferData(test::GetPromoCodeOfferData(GURL(kOfferUrl)));
  }

  // Simulates a navigation of `client`'s primary main frame to `url`.
  void NavigateTo(OfferNotificationHandler& handler,
                  MockAutofillClient& client,
                  const GURL& url) {
    client.set_last_committed_primary_main_frame_url(url);
    handler.UpdateOfferNotificationVisibility(client);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  MockAutofillClient client_;
  AutofillOfferManager offer_manager_{
      &client_.GetPersonalDataManager().test_payments_data_manager()};
  OfferNotificationHandler handler_{&offer_manager_};
};

// The notification is shown automatically only the first time.
TEST_F(OfferNotificationHandlerTest, ShowsAutomaticallyOnce) {
  AddOffer();

  EXPECT_CALL(client_.payments_autofill_client(),
              UpdateOfferNotification(_, ShowsAutomatically(true)));
  NavigateTo(handler_, client_, GURL(kOfferUrl));

  EXPECT_CALL(client_.payments_autofill_client(),
              UpdateOfferNotification(_, ShowsAutomatically(false)));
  NavigateTo(handler_, client_, GURL(kOfferUrl));
}

// A page without an offer dismisses the notification.
TEST_F(OfferNotificationHandlerTest, DismissesWithoutOffer) {
  AddOffer();

  EXPECT_CALL(client_.payments_autofill_client(), UpdateOfferNotification)
      .Times(0);
  EXPECT_CALL(client_.payments_autofill_client(), DismissOfferNotification);
  NavigateTo(handler_, client_, GURL(kOtherUrl));
}

// Hidden tabs must not set up an offer notification the user cannot see.
TEST_F(OfferNotificationHandlerTest, HiddenTab_DoesNotUpdateNotification) {
  client_.payments_autofill_client().set_is_tab_visible_for_offer_notification(
      false);
  AddOffer();

  EXPECT_CALL(client_.payments_autofill_client(), UpdateOfferNotification)
      .Times(0);
  EXPECT_CALL(client_.payments_autofill_client(), DismissOfferNotification)
      .Times(0);
  NavigateTo(handler_, client_, GURL(kOfferUrl));
}

// The automatic show is granted only once per offer. A hidden tab must not
// consume it on behalf of the tab the user is actually looking at.
TEST_F(OfferNotificationHandlerTest, HiddenTab_DoesNotConsumeAutomaticShow) {
  client_.payments_autofill_client().set_is_tab_visible_for_offer_notification(
      false);
  AddOffer();
  NavigateTo(handler_, client_, GURL(kOfferUrl));

  EXPECT_CALL(client_.payments_autofill_client(),
              UpdateOfferNotification(_, ShowsAutomatically(true)));
  client_.payments_autofill_client().set_is_tab_visible_for_offer_notification(
      true);
  NavigateTo(handler_, client_, GURL(kOfferUrl));
}

// The automatic show is shared across the tabs of a browser context.
TEST_F(OfferNotificationHandlerTest, ShowsAutomaticallyOncePerBrowserContext) {
  MockAutofillClient other_client;
  OfferNotificationHandler other_handler(&offer_manager_);
  AddOffer();
  NavigateTo(handler_, client_, GURL(kOfferUrl));

  EXPECT_CALL(other_client.payments_autofill_client(),
              UpdateOfferNotification(_, ShowsAutomatically(false)));
  NavigateTo(other_handler, other_client, GURL(kOfferUrl));
}

// The notification is dismissed rather than shown when the user's locale is not
// eligible for Wallet direct offers.
TEST_F(OfferNotificationHandlerTest, IneligibleLocale_DismissesNotification) {
  TestPaymentsDataManager ineligible_payments_data_manager(
      /*app_locale=*/"de-DE");
  ineligible_payments_data_manager.SetAutofillPaymentMethodsEnabled(true);
  ineligible_payments_data_manager.SetAutofillWalletImportEnabled(true);
  ineligible_payments_data_manager.AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL(kOfferUrl)));
  AutofillOfferManager ineligible_offer_manager(
      &ineligible_payments_data_manager);
  OfferNotificationHandler ineligible_handler(&ineligible_offer_manager);

  EXPECT_CALL(client_.payments_autofill_client(), UpdateOfferNotification)
      .Times(0);
  EXPECT_CALL(client_.payments_autofill_client(), DismissOfferNotification);
  NavigateTo(ineligible_handler, client_, GURL(kOfferUrl));
}

// The notification is dismissed rather than shown when Wallet import is
// disabled.
TEST_F(OfferNotificationHandlerTest,
       WalletImportDisabled_DismissesNotification) {
  AddOffer();
  client_.GetPersonalDataManager()
      .test_payments_data_manager()
      .SetAutofillWalletImportEnabled(false);

  EXPECT_CALL(client_.payments_autofill_client(), UpdateOfferNotification)
      .Times(0);
  EXPECT_CALL(client_.payments_autofill_client(), DismissOfferNotification);
  NavigateTo(handler_, client_, GURL(kOfferUrl));
}

}  // namespace
}  // namespace autofill
