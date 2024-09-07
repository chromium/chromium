// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
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

using testing::ElementsAre;
using testing::Pair;
using testing::Pointee;

namespace autofill {
namespace {

const char kTestGuid[] = "00000000-0000-0000-0000-000000000001";
const char kTestGuid2[] = "00000000-0000-0000-0000-000000000002";
const char kTestNumber[] = "4234567890123456";  // Visa
const char kTestUrl[] = "http://www.example.com/";
const char kTestUrlWithParam[] =
    "http://www.example.com/en/payments?name=checkout";
const char kOfferDetailsUrl[] = "http://pay.google.com";

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class AutofillOfferManagerTest : public testing::Test {
 public:
  AutofillOfferManagerTest() = default;
  ~AutofillOfferManagerTest() override = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());
    personal_data_manager_.SetSyncServiceForTest(&sync_service_);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());
    autofill_offer_manager_ =
        std::make_unique<AutofillOfferManager>(&personal_data_manager_);
  }

  CreditCard CreateCreditCard(std::string guid,
                              std::string number = kTestNumber,
                              int64_t instrument_id = 0) {
    CreditCard card = CreditCard();
    test::SetCreditCardInfo(&card, "Jane Doe", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    card.set_guid(guid);
    card.set_instrument_id(instrument_id);
    card.set_record_type(CreditCard::RecordType::kMaskedServerCard);

    personal_data_manager_.test_payments_data_manager().AddServerCreditCard(
        card);
    return card;
  }

  AutofillOfferData CreateCreditCardOfferForCard(
      const CreditCard& card,
      std::string offer_reward_amount,
      bool expired = false,
      std::vector<GURL> merchant_origins = {GURL(kTestUrl)}) {
    int64_t offer_id = 4444;
    base::Time expiry = expired ? AutofillClock::Now() - base::Days(2)
                                : AutofillClock::Now() + base::Days(2);
    std::vector<int64_t> eligible_instrument_id = {card.instrument_id()};
    GURL offer_details_url = GURL(kOfferDetailsUrl);
    DisplayStrings display_strings;
    display_strings.value_prop_text = "5% cash back when you use this card.";
    display_strings.see_details_text = "Terms apply.";
    display_strings.usage_instructions_text =
        "Check out with this card to activate.";

    AutofillOfferData offer_data = AutofillOfferData::GPayCardLinkedOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        eligible_instrument_id, offer_reward_amount);
    return offer_data;
  }

  AutofillOfferData CreatePromoCodeOffer(std::vector<GURL> merchant_origins = {
                                             GURL(kTestUrl)}) {
    int64_t offer_id = 5555;
    base::Time expiry = AutofillClock::Now() + base::Days(2);
    GURL offer_details_url = GURL(kOfferDetailsUrl);
    std::string promo_code = "5PCTOFFSHOES";
    DisplayStrings display_strings;
    display_strings.value_prop_text = "5% off on shoes. Up to $50.";
    display_strings.see_details_text = "See details";
    display_strings.usage_instructions_text =
        "Click the promo code field at checkout to autofill it.";
    std::string offer_reward_amount = "5%";

    AutofillOfferData offer_data = AutofillOfferData::GPayPromoCodeOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        offer_reward_amount);
    return offer_data;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillClient autofill_client_;
  syncer::TestSyncService sync_service_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that a card linked offer is returned for an eligible url.
TEST_F(AutofillOfferManagerTest, GetCardLinkedOffersMap_EligibleCashback) {
  CreditCard card = CreateCreditCard(kTestGuid);
  AutofillOfferData offer = CreateCreditCardOfferForCard(card, "5%");
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer);

  auto card_linked_offer_map =
      autofill_offer_manager_->GetCardLinkedOffersMap(GURL(kTestUrlWithParam));

  EXPECT_THAT(card_linked_offer_map,
              ElementsAre(Pair(card.guid(), Pointee(offer))));
}

// Verify that not expired offers are returned.
TEST_F(AutofillOfferManagerTest, GetCardLinkedOffersMap_ExpiredOffer) {
  CreditCard card = CreateCreditCard(kTestGuid);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      CreateCreditCardOfferForCard(card, "5%", /*expired=*/true));

  auto card_linked_offer_map =
      autofill_offer_manager_->GetCardLinkedOffersMap(GURL(kTestUrlWithParam));
  EXPECT_TRUE(card_linked_offer_map.empty());
}

// Verify that not offers are returned for a mismatching URL.
TEST_F(AutofillOfferManagerTest, GetCardLinkedOffersMap_WrongUrl) {
  CreditCard card = CreateCreditCard(kTestGuid);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      CreateCreditCardOfferForCard(card, "5%"));

  auto card_linked_offer_map = autofill_offer_manager_->GetCardLinkedOffersMap(
      GURL("http://wrongurl.com/"));
  EXPECT_TRUE(card_linked_offer_map.empty());
}

// Verify the card linked offer map returned contains only card linked offers,
// and no other types of offer (i.e. promo code offer).
TEST_F(AutofillOfferManagerTest, GetCardLinkedOffersMap_OnlyCardLinkedOffers) {
  CreditCard card1 = CreateCreditCard(kTestGuid, kTestNumber, 100);
  CreditCard card2 = CreateCreditCard(kTestGuid, "4111111111111111", 101);

  AutofillOfferData offer1 = CreateCreditCardOfferForCard(
      card1, "5%", /*expired=*/false,
      /*merchant_origins=*/
      {GURL("http://www.google.com"), GURL("http://www.youtube.com")});
  AutofillOfferData offer2 = CreateCreditCardOfferForCard(
      card2, "10%", /*expired=*/false,
      /*merchant_origins=*/
      {GURL("http://www.example.com"), GURL("http://www.example2.com")});
  AutofillOfferData offer3 =
      CreatePromoCodeOffer(/*merchant_origins=*/
                           {GURL("http://www.example.com"),
                            GURL("http://www.example2.com")});
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer1);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer2);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer3);

  auto card_linked_offer_map = autofill_offer_manager_->GetCardLinkedOffersMap(
      GURL("http://www.example.com"));
  ASSERT_EQ(card_linked_offer_map.size(), 1U);
  EXPECT_EQ(*card_linked_offer_map.at(card2.guid()), offer2);
}

// Verify that URLs with card linked offers available are marked as eligible.
TEST_F(AutofillOfferManagerTest, IsUrlEligible) {
  CreditCard card1 = CreateCreditCard(kTestGuid, kTestNumber, 100);
  CreditCard card2 = CreateCreditCard(kTestGuid2, "4111111111111111", 101);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      CreateCreditCardOfferForCard(
          card1, "5%", /*expired=*/false,
          {GURL("http://www.google.com"), GURL("http://www.youtube.com")}));
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      CreateCreditCardOfferForCard(card2, "10%", /*expired=*/false,
                                   {GURL("http://maps.google.com")}));
  autofill_offer_manager_->UpdateEligibleMerchantDomains();

  EXPECT_TRUE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.google.com")));
  EXPECT_FALSE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://www.example.com")));
  EXPECT_TRUE(
      autofill_offer_manager_->IsUrlEligible(GURL("http://maps.google.com")));
}

// Verify no offer is returned given a mismatch URL.
TEST_F(AutofillOfferManagerTest, GetOfferForUrl_ReturnNothingWhenFindNoMatch) {
  CreditCard card1 = CreateCreditCard(kTestGuid, kTestNumber, 100);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      CreateCreditCardOfferForCard(
          card1, "5%", /*expired=*/false,
          {GURL("http://www.google.com"), GURL("http://www.youtube.com")}));

  AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL("http://www.example.com"));
  EXPECT_EQ(nullptr, result);
}

// Verify the correct card linked offer is returned given an eligible URL.
TEST_F(AutofillOfferManagerTest,
       GetOfferForUrl_ReturnCorrectOfferWhenFindMatch) {
  CreditCard card1 = CreateCreditCard(kTestGuid, kTestNumber, 100);
  CreditCard card2 = CreateCreditCard(kTestGuid2, "4111111111111111", 101);

  AutofillOfferData offer1 = CreateCreditCardOfferForCard(
      card1, "5%", /*expired=*/false,
      /*merchant_origins=*/
      {GURL("http://www.google.com"), GURL("http://www.youtube.com")});
  AutofillOfferData offer2 = CreateCreditCardOfferForCard(
      card2, "10%", /*expired=*/false,
      /*merchant_origins=*/
      {GURL("http://www.example.com"), GURL("http://www.example2.com")});
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer1);
  personal_data_manager_.test_payments_data_manager().AddAutofillOfferData(
      offer2);

  AutofillOfferData* result =
      autofill_offer_manager_->GetOfferForUrl(GURL("http://www.example.com"));
  EXPECT_EQ(offer2, *result);
}

}  // namespace autofill
