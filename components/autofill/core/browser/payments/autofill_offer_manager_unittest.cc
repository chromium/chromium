// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::_;
using testing::ElementsAre;
using testing::Field;
using testing::Pair;
using testing::Pointee;

namespace autofill {
namespace {

const char kTestUrl[] = "http://www.example.com/";

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
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class AutofillOfferManagerTest : public testing::Test {
 public:
  AutofillOfferManagerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableWalletDirectOffers,
         features::kAutofillEnableWalletDirectOffersNotificationBubble},
        /*disabled_features=*/{});
  }
  ~AutofillOfferManagerTest() override = default;

  void SetUp() override {
    personal_data_manager().SetSyncServiceForTest(&sync_service_);
    autofill_offer_manager_ =
        std::make_unique<AutofillOfferManager>(&payments_data_manager());
  }

  // Simulates a navigation to `url` in the primary main frame.
  void NavigateTo(const GURL& url) {
    autofill_client_.set_last_committed_primary_main_frame_url(url);
    autofill_offer_manager_->UpdateOfferNotificationVisibility(
        autofill_client_);
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *autofill_client_.GetPaymentsAutofillClient());
  }

  TestPersonalDataManager& personal_data_manager() {
    return autofill_client_.GetPersonalDataManager();
  }

  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::TestSyncService sync_service_;
  MockAutofillClient autofill_client_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;
};

// Verify that URLs with promo code offers available are marked as eligible.
TEST_F(AutofillOfferManagerTest, IsUrlEligible) {
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL("http://www.google.com"),
                                  /*is_expired=*/false, /*offer_id=*/"google"));
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL("http://www.youtube.com"), /*is_expired=*/false,
      /*offer_id=*/"youtube"));

  EXPECT_TRUE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.google.com")));
  EXPECT_FALSE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.example.com")));
  EXPECT_TRUE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://maps.google.com")));
}

// Verify that URLs whose only offers are expired or have no promo code are not
// marked as eligible.
TEST_F(AutofillOfferManagerTest, IsUrlEligible_ExpiredOrNoPromoCode) {
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL("http://www.google.com"),
                                  /*is_expired=*/true, /*offer_id=*/"expired"));
  AutofillOfferData no_promo_code_offer = test::GetPromoCodeOfferData(
      GURL("http://www.youtube.com"), /*is_expired=*/false,
      /*offer_id=*/"no_promo_code");
  no_promo_code_offer.SetPromoCode("");
  payments_data_manager().AddAutofillOfferData(no_promo_code_offer);

  EXPECT_FALSE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.google.com")));
  EXPECT_FALSE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.youtube.com")));
}

// Verify no offer is returned given a mismatch URL.
TEST_F(AutofillOfferManagerTest, GetOfferForUrl_ReturnNothingWhenFindNoMatch) {
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL("http://www.google.com"),
                                  /*is_expired=*/false, /*offer_id=*/"google"));

  const AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL("http://www.example.com"));
  EXPECT_EQ(nullptr, result);
}

// Verify the correct promo code offer is returned given an eligible URL.
TEST_F(AutofillOfferManagerTest,
       GetOfferForUrl_ReturnCorrectOfferWhenFindMatch) {
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL("http://www.google.com"),
                                  /*is_expired=*/false, /*offer_id=*/"google"));
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL("http://www.example.com"), /*is_expired=*/false,
      /*offer_id=*/"example"));

  const AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL("http://www.example.com"));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->GetOfferId(), "example");
}

// Verify that offers which are expired or have no promo code are skipped, even
// if they come before an eligible offer.
TEST_F(AutofillOfferManagerTest, GetOfferForUrl_SkipsExpiredOrNoPromoCode) {
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL(kTestUrl), /*is_expired=*/true, /*offer_id=*/"expired"));
  AutofillOfferData no_promo_code_offer = test::GetPromoCodeOfferData(
      GURL(kTestUrl), /*is_expired=*/false, /*offer_id=*/"no_promo_code");
  no_promo_code_offer.SetPromoCode("");
  payments_data_manager().AddAutofillOfferData(no_promo_code_offer);
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL(kTestUrl), /*is_expired=*/false, /*offer_id=*/"eligible"));

  const AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL(kTestUrl));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->GetOfferId(), "eligible");
}

// Verify that shown notifications are remembered per offer.
TEST_F(AutofillOfferManagerTest, MarkNotificationShown) {
  EXPECT_FALSE(autofill_offer_manager_->HasShownNotification("1"));

  autofill_offer_manager_->MarkNotificationShown("1");

  EXPECT_TRUE(autofill_offer_manager_->HasShownNotification("1"));
  EXPECT_FALSE(autofill_offer_manager_->HasShownNotification("2"));
}

// Hidden tabs must not set up an offer notification the user cannot see.
TEST_F(AutofillOfferManagerTest, HiddenTab_DoesNotUpdateNotification) {
  payments_autofill_client().set_is_tab_visible_for_offer_notification(false);
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL(kTestUrl)));

  EXPECT_CALL(payments_autofill_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_autofill_client(), DismissOfferNotification).Times(0);

  NavigateTo(GURL(kTestUrl));
}

// The automatic show is granted only once per offer. A hidden tab must not
// consume it on behalf of the tab the user is actually looking at.
TEST_F(AutofillOfferManagerTest, HiddenTab_DoesNotConsumeAutomaticShow) {
  payments_autofill_client().set_is_tab_visible_for_offer_notification(false);
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL(kTestUrl)));
  NavigateTo(GURL(kTestUrl));

  EXPECT_CALL(
      payments_autofill_client(),
      UpdateOfferNotification(
          _, Field(&OfferNotificationOptions::show_notification_automatically,
                   true)));

  payments_autofill_client().set_is_tab_visible_for_offer_notification(true);
  NavigateTo(GURL(kTestUrl));
}

// Verify that if several offers apply to a URL, the first matching one in the
// order returned by `PaymentsDataManager::GetAutofillOffers()` is returned.
TEST_F(AutofillOfferManagerTest, GetOfferForUrl_ReturnsFirstMatchingOffer) {
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL("https://www.other.com/"), /*is_expired=*/false, "other_site"));
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL(kTestUrl), /*is_expired=*/false, "first_match"));
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL(kTestUrl), /*is_expired=*/false, "second_match"));

  const AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL(kTestUrl));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->GetOfferId(), "first_match");
}

// Verify that a Wallet direct offer applies to any URL with the same eTLD+1 as
// its merchant origin, but not to look-alike hosts.
TEST_F(AutofillOfferManagerTest, WalletDirectOffer_MatchesSameDomain) {
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL("https://example.com/"), /*is_expired=*/false, "offer"));

  for (const char* url :
       {"https://example.com/", "https://example.com/shop?item=1#top",
        "https://www.example.com/", "https://store.example.com/shop",
        "https://a.b.example.com/"}) {
    SCOPED_TRACE(url);
    EXPECT_TRUE(autofill_offer_manager_->IsUrlEligible(GURL(url)));
    const AutofillOfferData* offer =
        autofill_offer_manager_->GetOfferForUrl(GURL(url));
    ASSERT_TRUE(offer);
    EXPECT_EQ(offer->GetOfferId(), "offer");
  }

  for (const char* url :
       {"https://notexample.com/", "https://example.com.evil.com/",
        "https://example.org/"}) {
    SCOPED_TRACE(url);
    EXPECT_FALSE(autofill_offer_manager_->IsUrlEligible(GURL(url)));
    EXPECT_FALSE(autofill_offer_manager_->GetOfferForUrl(GURL(url)));
  }
}

// Verify that a Wallet direct offer for a subdomain also applies to its parent
// domain and to sibling subdomains.
TEST_F(AutofillOfferManagerTest,
       WalletDirectOffer_SubdomainMatchesParentAndSiblings) {
  payments_data_manager().AddAutofillOfferData(test::GetPromoCodeOfferData(
      GURL("https://store.example.com/"), /*is_expired=*/false, "offer"));

  for (const char* url : {"https://store.example.com/shop",
                          "https://example.com/", "https://www.example.com/"}) {
    SCOPED_TRACE(url);
    EXPECT_TRUE(autofill_offer_manager_->IsUrlEligible(GURL(url)));
  }
  EXPECT_FALSE(
      autofill_offer_manager_->IsUrlEligible(GURL("https://notexample.com/")));
}

// Verify that no URL is eligible, and the offer notification is dismissed
// rather than shown, when the user's locale is not eligible for Wallet direct
// offers. This matches the eligibility used for promo code suggestions.
TEST_F(AutofillOfferManagerTest, IneligibleLocale_DoesNotShowNotification) {
  TestPaymentsDataManager ineligible_payments_data_manager(
      /*app_locale=*/"de-DE");
  ineligible_payments_data_manager.SetAutofillPaymentMethodsEnabled(true);
  ineligible_payments_data_manager.SetAutofillWalletImportEnabled(true);
  ineligible_payments_data_manager.AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL(kTestUrl)));
  ASSERT_TRUE(ineligible_payments_data_manager
                  .GetActiveAutofillPromoCodeOffersForOrigin(GURL(kTestUrl))
                  .empty());

  AutofillOfferManager offer_manager(&ineligible_payments_data_manager);
  EXPECT_FALSE(offer_manager.IsUrlEligible(GURL(kTestUrl)));

  EXPECT_CALL(payments_autofill_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_autofill_client(), DismissOfferNotification);
  autofill_client_.set_last_committed_primary_main_frame_url(GURL(kTestUrl));
  offer_manager.UpdateOfferNotificationVisibility(autofill_client_);
}

// Verify that the offer notification is not shown when Wallet import is
// disabled, matching the eligibility used for promo code suggestions.
TEST_F(AutofillOfferManagerTest, WalletImportDisabled_DoesNotShowNotification) {
  payments_data_manager().AddAutofillOfferData(
      test::GetPromoCodeOfferData(GURL(kTestUrl)));
  payments_data_manager().SetAutofillWalletImportEnabled(false);

  EXPECT_FALSE(autofill_offer_manager_->IsUrlEligible(GURL(kTestUrl)));
  EXPECT_CALL(payments_autofill_client(), UpdateOfferNotification).Times(0);
  EXPECT_CALL(payments_autofill_client(), DismissOfferNotification);
  NavigateTo(GURL(kTestUrl));
}

}  // namespace autofill
