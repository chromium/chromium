// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
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

  // Lets tests share an offer manager between clients, like the tabs of a
  // browser context do.
  AutofillOfferManager* GetAutofillOfferManager() override {
    return offer_manager_;
  }
  void set_offer_manager(AutofillOfferManager* offer_manager) {
    offer_manager_ = offer_manager;
  }

 private:
  raw_ptr<AutofillOfferManager> offer_manager_ = nullptr;
};

class MockAutofillClient : public TestContentAutofillClient {
 public:
  explicit MockAutofillClient(content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    set_payments_autofill_client(
        std::make_unique<testing::NiceMock<MockPaymentsAutofillClient>>(this));
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *GetPaymentsAutofillClient());
  }
};

// Lets tests control whether the bubble is reported as showing.
class TestOfferNotificationHandler : public OfferNotificationHandler {
 public:
  using OfferNotificationHandler::OfferNotificationHandler;

  void set_is_bubble_showing(bool is_bubble_showing) {
    is_bubble_showing_ = is_bubble_showing;
  }

 private:
  bool IsOfferNotificationBubbleShowing() const override {
    return is_bubble_showing_;
  }

  bool is_bubble_showing_ = false;
};

auto ShowsAutomatically(bool show_automatically) {
  return Field(&OfferNotificationOptions::show_notification_automatically,
               show_automatically);
}

class OfferNotificationHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  OfferNotificationHandlerTest() {
    feature_list_.InitWithFeatures(
        {features::kAutofillEnableWalletDirectOffers,
         features::kAutofillEnableWalletDirectOffersNotificationBubble},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    client().GetPersonalDataManager().SetSyncServiceForTest(&sync_service_);
    offer_manager_ = std::make_unique<AutofillOfferManager>(
        &client().GetPersonalDataManager().test_payments_data_manager());
    payments_client().set_offer_manager(offer_manager_.get());
    web_contents()->WasShown();
  }

  void TearDown() override {
    payments_client().set_offer_manager(nullptr);
    offer_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<TestOfferNotificationHandler> CreateHandler(
      MockAutofillClient& client) {
    return std::make_unique<TestOfferNotificationHandler>(client);
  }

  MockAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }

  void AddOffer() {
    client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .AddAutofillOfferData(test::GetPromoCodeOfferData(GURL(kOfferUrl)));
  }

  MockPaymentsAutofillClient& payments_client() {
    return client().payments_autofill_client();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;
  TestAutofillClientInjector<MockAutofillClient> autofill_client_injector_;
  std::unique_ptr<AutofillOfferManager> offer_manager_;
};

// The notification is shown automatically only the first time.
TEST_F(OfferNotificationHandlerTest, Navigation_ShowsAutomaticallyOnce) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  AddOffer();

  EXPECT_CALL(payments_client(),
              UpdateOfferNotification(_, ShowsAutomatically(true)));
  NavigateAndCommit(GURL(kOfferUrl));

  EXPECT_CALL(payments_client(),
              UpdateOfferNotification(_, ShowsAutomatically(false)));
  NavigateAndCommit(GURL(kOfferUrl));
}

// Navigating to a page without an offer dismisses the notification.
TEST_F(OfferNotificationHandlerTest, Navigation_DismissesWithoutOffer) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  AddOffer();

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification);
  NavigateAndCommit(GURL(kOtherUrl));
}

// Offers arriving after the navigation update the notification.
TEST_F(OfferNotificationHandlerTest, PaymentsDataChanged_UpdatesNotification) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  NavigateAndCommit(GURL(kOfferUrl));

  EXPECT_CALL(payments_client(),
              UpdateOfferNotification(_, ShowsAutomatically(true)));
  AddOffer();
}

// A showing bubble is left alone when the payments data changes, since
// re-evaluating would collapse it to its icon.
TEST_F(OfferNotificationHandlerTest,
       PaymentsDataChanged_SkipsWhileBubbleShowing) {
  std::unique_ptr<TestOfferNotificationHandler> handler =
      CreateHandler(client());
  NavigateAndCommit(GURL(kOfferUrl));
  handler->set_is_bubble_showing(true);

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification).Times(0);
  AddOffer();
}

// Navigations still update the notification while its bubble is showing.
TEST_F(OfferNotificationHandlerTest, Navigation_UpdatesWhileBubbleShowing) {
  std::unique_ptr<TestOfferNotificationHandler> handler =
      CreateHandler(client());
  AddOffer();
  handler->set_is_bubble_showing(true);

  EXPECT_CALL(payments_client(), DismissOfferNotification);
  NavigateAndCommit(GURL(kOtherUrl));
}

// Same-document navigations, e.g. to a fragment, don't update the notification,
// which would collapse a showing bubble to its icon.
TEST_F(OfferNotificationHandlerTest, SameDocumentNavigation_DoesNotUpdate) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  AddOffer();
  NavigateAndCommit(GURL(kOfferUrl));

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification).Times(0);
  content::NavigationSimulator::CreateRendererInitiated(
      GURL(kOfferUrl).Resolve("#section"), main_rfh())
      ->CommitSameDocument();
}

// Hidden tabs must not set up an offer notification the user cannot see.
TEST_F(OfferNotificationHandlerTest, HiddenTab_DoesNotUpdateNotification) {
  web_contents()->WasHidden();
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification).Times(0);
  AddOffer();
  NavigateAndCommit(GURL(kOfferUrl));
}

// A hidden tab must not consume the automatic show, and the notification is
// set up once the tab is shown.
TEST_F(OfferNotificationHandlerTest, HiddenTab_ShowsAutomaticallyWhenShown) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  web_contents()->WasHidden();
  AddOffer();
  NavigateAndCommit(GURL(kOfferUrl));

  EXPECT_CALL(payments_client(),
              UpdateOfferNotification(_, ShowsAutomatically(true)));
  web_contents()->WasShown();
}

// The automatic show is shared across the tabs of a browser context.
TEST_F(OfferNotificationHandlerTest, ShowsAutomaticallyOncePerBrowserContext) {
  std::unique_ptr<content::WebContents> other_web_contents =
      CreateTestWebContents();
  other_web_contents->WasShown();
  MockAutofillClient& other_client =
      *autofill_client_injector_[other_web_contents.get()];
  other_client.payments_autofill_client().set_offer_manager(
      offer_manager_.get());
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  std::unique_ptr<OfferNotificationHandler> other_handler =
      CreateHandler(other_client);
  AddOffer();
  NavigateAndCommit(GURL(kOfferUrl));

  EXPECT_CALL(other_client.payments_autofill_client(),
              UpdateOfferNotification(_, ShowsAutomatically(false)));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      other_web_contents.get(), GURL(kOfferUrl));
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
  payments_client().set_offer_manager(&ineligible_offer_manager);
  TestOfferNotificationHandler ineligible_handler(client());

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification);
  NavigateAndCommit(GURL(kOfferUrl));

  // Restore it before `ineligible_offer_manager` is destroyed.
  payments_client().set_offer_manager(offer_manager_.get());
}

// The notification is dismissed rather than shown when Wallet import is
// disabled.
TEST_F(OfferNotificationHandlerTest,
       WalletImportDisabled_DismissesNotification) {
  std::unique_ptr<OfferNotificationHandler> handler = CreateHandler(client());
  AddOffer();
  client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .SetAutofillWalletImportEnabled(false);

  EXPECT_CALL(payments_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_client(), DismissOfferNotification);
  NavigateAndCommit(GURL(kOfferUrl));
}

}  // namespace
}  // namespace autofill
