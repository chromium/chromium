// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

// TODO(crbug.com/1196021): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class AutofillSuggestionGeneratorTest : public testing::Test {
 public:
  AutofillSuggestionGeneratorTest() {
    scoped_feature_list_async_parse_form_.InitWithFeatureState(
        features::kAutofillParseAsync, true);
  }

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.Init(/*profile_database=*/database_,
                        /*account_database=*/nullptr,
                        /*pref_service=*/autofill_client_.GetPrefs(),
                        /*local_state=*/autofill_client_.GetPrefs(),
                        /*identity_manager=*/nullptr,
                        /*history_service=*/nullptr,
                        /*strike_database=*/nullptr,
                        /*image_fetcher=*/nullptr,
                        /*is_off_the_record=*/false);
    suggestion_generator_ = std::make_unique<AutofillSuggestionGenerator>(
        &autofill_client_, &personal_data_);
    autofill_client_.set_autofill_offer_manager(
        std::make_unique<AutofillOfferManager>(
            &personal_data_,
            /*coupon_service_delegate=*/nullptr));
  }

  AutofillSuggestionGenerator* suggestion_generator() {
    return suggestion_generator_.get();
  }

  TestPersonalDataManager* personal_data() { return &personal_data_; }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillEnvironment autofill_environment_;
  std::unique_ptr<AutofillSuggestionGenerator> suggestion_generator_;
  TestAutofillClient autofill_client_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_;
};

TEST_F(AutofillSuggestionGeneratorTest,
       RemoveExpiredCreditCardsNotUsedSinceTimestamp) {
  const char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  const base::Time kNow = AutofillClock::Now();
  constexpr size_t kNumCards = 10;

  // We construct a card vector as below, number indicate days of last used
  // from |kNow|:
  // [30, 90, 150, 210, 270, 0, 60, 120, 180, 240]
  // |expires at 2999     |, |expired at 2001   |
  std::vector<CreditCard> all_card_data;
  std::vector<CreditCard*> all_card_ptrs;
  all_card_data.reserve(kNumCards);
  all_card_ptrs.reserve(kNumCards);
  for (size_t i = 0; i < kNumCards; ++i) {
    constexpr base::TimeDelta k30Days = base::Days(30);
    all_card_data.emplace_back(base::GenerateGUID(), "https://example.com");
    if (i < 5) {
      all_card_data.back().set_use_date(kNow - (i + i + 1) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "Clyde Barrow",
                              "378282246310005" /* American Express */, "04",
                              "2999", "1");
    } else {
      all_card_data.back().set_use_date(kNow - (i + i - 10) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "John Dillinger",
                              "4234567890123456" /* Visa */, "04", "2001", "1");
    }
    all_card_ptrs.push_back(&all_card_data.back());
  }

  // Verify that only expired disused card are removed. Note that only the last
  // two cards have use dates more than 175 days ago and are expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // The first 8 are either not expired or having use dates more recent
    // than 175 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin(), cards.begin() + 8);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::Days(175), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  }

  // Reverse the card order and verify that only expired and disused cards
  // are removed. Note that the first three cards, post reversal,
  // have use dates more then 115 days ago.
  {
    // Create a reversed working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs.rbegin(),
                                   all_card_ptrs.rend());

    // The last 7 cards have use dates more recent than 115 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin() + 3, cards.end());

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::Days(115), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }
  // Randomize the card order and validate that the filtered list retains
  // that order. Note that the three cards have use dates more then 115
  // days ago and are expired.
  {
    // A handy constant.
    const base::Time k115DaysAgo = kNow - base::Days(115);

    // Created a shuffled primary copy of the card pointers.
    std::vector<CreditCard*> shuffled_cards(all_card_ptrs);
    base::RandomShuffle(shuffled_cards.begin(), shuffled_cards.end());

    // Copy the shuffled card pointer collections to use as the working
    // set.
    std::vector<CreditCard*> cards(shuffled_cards);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, k115DaysAgo, &cards);

    // Validate that we have the right cards. Iterate of the the shuffled
    // primary copy and the filtered copy at the same time. making sure that
    // the elements in the filtered copy occur in the same order as the shuffled
    // primary. Along the way, validate that the elements in and out of the
    // filtered copy have appropriate use dates and expiration states.
    EXPECT_EQ(7u, cards.size());
    auto it = shuffled_cards.begin();
    for (const CreditCard* card : cards) {
      for (; it != shuffled_cards.end() && (*it) != card; ++it) {
        EXPECT_LT((*it)->use_date(), k115DaysAgo);
        ASSERT_TRUE((*it)->IsExpired(kNow));
      }
      ASSERT_TRUE(it != shuffled_cards.end());
      ASSERT_TRUE(card->use_date() > k115DaysAgo || !card->IsExpired(kNow));
      ++it;
    }
    for (; it != shuffled_cards.end(); ++it) {
      EXPECT_LT((*it)->use_date(), k115DaysAgo);
      ASSERT_TRUE((*it)->IsExpired(kNow));
    }

    // Validate the histograms.
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }

  // Verify all cards are retained if they're sufficiently recently
  // used.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow - base::Days(720), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(all_card_ptrs, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 0, 1);
  }

  // Verify all cards are removed if they're all disused and expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);
    for (auto it = all_card_ptrs.begin(); it < all_card_ptrs.end(); it++) {
      (*it)->SetExpirationYear(2001);
    }

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow + base::Days(1), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_TRUE(cards.empty());
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, kNumCards, 1);
  }

  // Verify all expired and disused server cards are not removed.
  {
    // Create a working copy of the card pointers. And set one card to be a
    // masked server card.
    std::vector<CreditCard*> cards(all_card_ptrs);
    for (auto it = all_card_ptrs.begin(); it < all_card_ptrs.end(); it++) {
      (*it)->SetExpirationYear(2001);
    }
    cards[0]->set_record_type(CreditCard::MASKED_SERVER_CARD);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
        kNow, kNow + base::Days(1), &cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(1U, cards.size());
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, kNumCards - 1, 1);
  }
}

TEST_F(AutofillSuggestionGeneratorTest, GetServerCardForLocalCard) {
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  test::SetCreditCardInfo(&server_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");
  personal_data()->AddServerCreditCard(server_card);

  CreditCard local_card("00000000-0000-0000-0000-000000000002",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");

  // The server card should be returned if the local card is passed in.
  const CreditCard* result =
      suggestion_generator()->GetServerCardForLocalCard(&local_card);
  ASSERT_TRUE(result);
  EXPECT_EQ(server_card.guid(), result->guid());

  // Should return nullptr if a server card is passed in.
  EXPECT_FALSE(suggestion_generator()->GetServerCardForLocalCard(&server_card));

  // Should return nullptr if no server card has the same information as the
  // local card.
  server_card.SetNumber(u"5454545454545454");
  personal_data()->ClearCreditCards();
  personal_data()->AddServerCreditCard(server_card);
  EXPECT_FALSE(suggestion_generator()->GetServerCardForLocalCard(&local_card));
}

// The suggestions of credit cards with card linked offers are moved to the
// front. This test checks that the order of the other cards remains stable.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForCreditCards_StableSortBasedOnOffer) {
  // Create three server cards.
  CreditCard server_card1 = test::GetMaskedServerCard();
  server_card1.set_server_id("server_id1");
  server_card1.set_guid("00000000-0000-0000-0000-000000000001");
  server_card1.set_instrument_id(1);
  personal_data()->AddServerCreditCard(server_card1);

  CreditCard server_card2 = test::GetMaskedServerCard();
  server_card2.set_server_id("server_id2");
  server_card2.set_guid("00000000-0000-0000-0000-000000000002");
  server_card2.set_instrument_id(2);
  personal_data()->AddServerCreditCard(server_card2);

  CreditCard server_card3 = test::GetMaskedServerCard();
  server_card3.set_server_id("server_id3");
  server_card3.set_guid("00000000-0000-0000-0000-000000000003");
  server_card3.set_instrument_id(3);
  personal_data()->AddServerCreditCard(server_card3);

  // Create a card linked offer and attach it to server_card2.
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  offer_data.SetMerchantOriginForTesting({GURL("http://www.example1.com")});
  offer_data.SetEligibleInstrumentIdForTesting({2});
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("http://www.example1.com"));
  personal_data()->AddAutofillOfferData(offer_data);

  // Create a credit card form.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormStructure form_structure(credit_card_form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  bool should_display_gpay_logo = false;
  bool with_offer = false;
  auto suggestions = suggestion_generator()->GetSuggestionsForCreditCards(
      FormFieldData(), AutofillType(CREDIT_CARD_NUMBER),
      /*app_locale=*/"en", &should_display_gpay_logo, &with_offer);

  EXPECT_TRUE(with_offer);
  ASSERT_EQ(suggestions.size(), 3U);
  // The suggestion with card linked offer available should be ranked to the
  // top.
  EXPECT_EQ(suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000002"));
  // The other suggestions should have their relative ranking unchanged.
  EXPECT_EQ(suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000003"));
  EXPECT_EQ(suggestions[2].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));
}

TEST_F(AutofillSuggestionGeneratorTest, CreateCreditCardSuggestion_ServerCard) {
  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true, "",
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.frontend_id,
            POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "", /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));
}

// Test to make sure the suggestion gets populated with the right content if the
// card has card linked offer available.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_ServerCardWithOffer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kAutofillKeyboardAccessory,
           features::kAutofillEnableOffersInClankKeyboardAccessory});
  const bool kKeyboardAccessoryEnabledOptions[] = {
    false,
#if BUILDFLAG(IS_ANDROID)
    true
#endif
  };

  for (bool keyboard_accessory_enabled : kKeyboardAccessoryEnabledOptions) {
#if BUILDFLAG(IS_ANDROID)
    if (keyboard_accessory_enabled) {
      feature_list.Reset();
      feature_list.InitWithFeatures(
          {features::kAutofillKeyboardAccessory,
           features::kAutofillEnableOffersInClankKeyboardAccessory},
          {});
    }
#endif

    // Create a server card.
    CreditCard server_card1 = test::GetMaskedServerCard();
    server_card1.set_server_id("server_id1");
    server_card1.set_guid("00000000-0000-0000-0000-000000000001");
    server_card1.set_virtual_card_enrollment_state(
        CreditCard::VirtualCardEnrollmentState::ENROLLED);

    Suggestion virtual_card_suggestion =
        suggestion_generator()->CreateCreditCardSuggestion(
            server_card1, AutofillType(CREDIT_CARD_NUMBER),
            /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true,
            "",
            /*card_linked_offer_available=*/true);

    EXPECT_EQ(virtual_card_suggestion.frontend_id,
              POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);
    EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
              Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));
    // Ensures CLO text is not shown for virtual card option.
    EXPECT_EQ(virtual_card_suggestion.labels.size(), 1U);

    Suggestion real_card_suggestion =
        suggestion_generator()->CreateCreditCardSuggestion(
            server_card1, AutofillType(CREDIT_CARD_NUMBER),
            /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
            "", /*card_linked_offer_available=*/true);

    EXPECT_EQ(real_card_suggestion.frontend_id, 0);
    EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
              Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));

    if (keyboard_accessory_enabled) {
#if BUILDFLAG(IS_ANDROID)
      EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
      EXPECT_EQ(
          real_card_suggestion.feature_for_iph,
          feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature.name);
#endif
    } else {
      ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
      ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
      EXPECT_EQ(real_card_suggestion.labels[1][0].value,
                l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
    }
  }
}

TEST_F(AutofillSuggestionGeneratorTest, CreateCreditCardSuggestion_LocalCard) {
  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  test::SetCreditCardInfo(&server_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");
  personal_data()->AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card("00000000-0000-0000-0000-000000000002",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true, "",
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.frontend_id,
            POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000001"));

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "", /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("00000000-0000-0000-0000-000000000002"));
  EXPECT_TRUE(real_card_suggestion.custom_icon.IsEmpty());
}

// Credit card name field suggestion with metadata for virtual cards in Autofill
// popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_VirtualCardNameField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableVirtualCardMetadata);

  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  test::SetCreditCardInfo(&server_card, "Mojo Jojo", "4111111111111111", "04",
                          test::NextYear().c_str(), "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NAME_FULL),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true, "",
          /*card_linked_offer_available=*/false);

  // "Virtual card" text is prefixed to the name.
  EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
            u"Virtual card");
  EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"Mojo Jojo");

#if BUILDFLAG(IS_ANDROID)
  // For Android, the label is "Network ....1234".
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
#elif BUILDFLAG(IS_IOS)
  // For IOS, the label is "....1234".
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Network ....1234, expires on mm/yy".
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      virtual_card_name_field_suggestion.labels[0][0].value,
      base::StrCat({u"Visa  ",
                    internal::GetObfuscatedStringForCardDigits(u"1111", 4),
                    u", expires on 04/",
                    base::UTF8ToUTF16(test::NextYear().substr(2))}));
#endif
}

// Credit card number field suggestion with metadata for virtual cards in
// Autofill popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_VirtualCardNumberField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableVirtualCardMetadata);

  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  test::SetCreditCardInfo(&server_card, "Mojo Jojo", "4111111111111111", "04",
                          test::NextYear().c_str(), "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true, "",
          /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_ANDROID)
  // For Android, split the first line and populate card name, last 4 digits
  // separately.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // Only card number is displayed on the first line.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value, u"");
#endif

  // "Virtual card" is the label.
  ASSERT_EQ(virtual_card_number_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_number_field_suggestion.labels[0][0].value,
            u"Virtual card");
}

// Credit card name field suggestion with metadata for non-virtual cards in
// Autofill popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_NonVirtualCardNameField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableVirtualCardMetadata);

  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  test::SetCreditCardInfo(&server_card, "Mojo Jojo", "4111111111111111", "04",
                          test::NextYear().c_str(), "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);

  // Name field suggestion for non-virtual cards.
  Suggestion real_card_name_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NAME_FULL),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "", /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Mojo Jojo");
  EXPECT_EQ(real_card_name_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_ANDROID)
  // For Android, the label is "Network ....1234".
  ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
#elif BUILDFLAG(IS_IOS)
  // For IOS, the label is "....1234".
  ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Network ....1234, expires on mm/yy".
  ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      real_card_name_field_suggestion.labels[0][0].value,
      base::StrCat({u"Visa  ",
                    internal::GetObfuscatedStringForCardDigits(u"1111", 4),
                    u", expires on 04/",
                    base::UTF8ToUTF16(test::NextYear().substr(2))}));
#endif
}

// Credit card number field suggestion with metadata for non-virtual cards in
// Autofill popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_NonVirtualCardNumberField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableVirtualCardMetadata);

  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  test::SetCreditCardInfo(&server_card, "Mojo Jojo", "4111111111111111", "04",
                          test::NextYear().c_str(), "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);

  // Card number field suggestion for non-virtual cards.
  Suggestion real_card_number_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "", /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_ANDROID)
  // For Android, split the first line and populate card name, last 4 digits
  // separately.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // Only the card number is displayed on the first line.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value, u"");
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // For mobile devices, the label is the expiration date formatted as mm/yy.
  ASSERT_EQ(real_card_number_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      real_card_number_field_suggestion.labels[0][0].value,
      base::StrCat({u"04/", base::UTF8ToUTF16(test::NextYear().substr(2))}));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Expires on mm/yy".
  ASSERT_EQ(real_card_number_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(real_card_number_field_suggestion.labels[0][0].value,
            base::StrCat({u"Expires on 04/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))}));
#endif
}

TEST_F(AutofillSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
  // Create a complete form.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormStructure form_structure(credit_card_form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < credit_card_form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure.field(i)->heuristic_type());
  }
  FormStructureTestApi(&form_structure)
      .SetFieldTypes(heuristic_types, server_types);

  // Create a server card.
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_server_id("server_id1");
  server_card.set_guid("00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  test::SetCreditCardInfo(&server_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");
  personal_data()->AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card("00000000-0000-0000-0000-000000000002",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Elvis Presley", "4111111111111111",
                          "04", test::NextYear().c_str(), "1");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(&local_card));

  // Reset server card virtual card enrollment state.
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNSPECIFIED);
  personal_data()->ClearCreditCards();
  personal_data()->AddServerCreditCard(server_card);

  // For server card not enrolled, should return false.
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&local_card));

  // Remove the server credit card.
  personal_data()->ClearCreditCards();

  // The local card no longer has a server duplicate, should return false.
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

TEST_F(AutofillSuggestionGeneratorTest, GetIBANSuggestions) {
  auto MakeIBAN = [](const std::u16string& value,
                     const std::u16string& nickname) {
    IBAN iban(base::GenerateGUID());
    iban.set_value(value);
    if (!nickname.empty())
      iban.set_nickname(nickname);
    return iban;
  };
  IBAN iban0 = MakeIBAN(u"CH56 0483 5012 3456 7800 9", u"My doctor's IBAN");
  IBAN iban1 = MakeIBAN(u"DE91 1000 0000 0123 4567 89", u"My brother's IBAN");
  IBAN iban2 =
      MakeIBAN(u"GR96 0810 0010 0000 0123 4567 890", u"My teacher's IBAN");
  IBAN iban3 = MakeIBAN(u"PK70 BANK 0000 1234 5678 9000", u"");

  std::vector<Suggestion> iban_suggestions =
      AutofillSuggestionGenerator::GetSuggestionsForIBANs(
          {&iban0, &iban1, &iban2, &iban3});
  EXPECT_TRUE(iban_suggestions.size() == 4);

  EXPECT_EQ(iban_suggestions[0].main_text.value,
            iban0.GetIdentifierStringForAutofillDisplay());
  ASSERT_EQ(iban_suggestions[0].labels.size(), 1u);
  ASSERT_EQ(iban_suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(iban_suggestions[0].labels[0][0].value, u"My doctor's IBAN");
  EXPECT_EQ(iban_suggestions[0].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[1].main_text.value,
            iban1.GetIdentifierStringForAutofillDisplay());
  ASSERT_EQ(iban_suggestions[1].labels.size(), 1u);
  ASSERT_EQ(iban_suggestions[1].labels[0].size(), 1u);
  EXPECT_EQ(iban_suggestions[1].labels[0][0].value, u"My brother's IBAN");
  EXPECT_EQ(iban_suggestions[1].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[2].main_text.value,
            iban2.GetIdentifierStringForAutofillDisplay());
  ASSERT_EQ(iban_suggestions[2].labels.size(), 1u);
  ASSERT_EQ(iban_suggestions[2].labels[0].size(), 1u);
  EXPECT_EQ(iban_suggestions[2].labels[0][0].value, u"My teacher's IBAN");
  EXPECT_EQ(iban_suggestions[2].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[3].main_text.value,
            iban3.GetIdentifierStringForAutofillDisplay());
  EXPECT_EQ(iban_suggestions[3].labels.size(), 0u);
  EXPECT_EQ(iban_suggestions[3].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  std::vector<const AutofillOfferData*> promo_code_offers;

  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/1, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  promo_code_offers.push_back(&offer1);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/2, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  promo_code_offers.push_back(&offer2);

  std::vector<Suggestion> promo_code_suggestions =
      AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
          promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 4);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("1"));
  ASSERT_EQ(promo_code_suggestions[0].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[0].labels[0][0].value,
            u"test_value_prop_text_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("1"));
  EXPECT_EQ(promo_code_suggestions[0].frontend_id,
            POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("2"));
  ASSERT_EQ(promo_code_suggestions[1].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[1].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[1].labels[0][0].value,
            u"test_value_prop_text_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId("2"));
  EXPECT_EQ(promo_code_suggestions[1].frontend_id,
            POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);

  EXPECT_EQ(promo_code_suggestions[2].frontend_id, POPUP_ITEM_ID_SEPARATOR);

  EXPECT_EQ(promo_code_suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
  EXPECT_EQ(promo_code_suggestions[3].GetPayload<GURL>(),
            offer1.GetOfferDetailsUrl().spec());
  EXPECT_EQ(promo_code_suggestions[3].frontend_id,
            POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS);
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_InvalidPromoCodeURL) {
  std::vector<const AutofillOfferData*> promo_code_offers;
  AutofillOfferData offer;
  offer.SetPromoCode("test_promo_code_1");
  offer.SetValuePropTextInDisplayStrings("test_value_prop_text_1");
  offer.SetOfferIdForTesting(1);
  offer.SetOfferDetailsUrl(GURL("invalid-url"));
  promo_code_offers.push_back(&offer);

  std::vector<Suggestion> promo_code_suggestions =
      AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
          promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 1);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  ASSERT_EQ(promo_code_suggestions[0].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[0].labels[0][0].value,
            u"test_value_prop_text_1");
  EXPECT_FALSE(
      absl::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].frontend_id,
            POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);
}

TEST_F(AutofillSuggestionGeneratorTest, BackendIdAndInternalIdMappings) {
  // Test that internal ID retrieval with an invalid backend ID works correctly.
  Suggestion::BackendId backend_id = Suggestion::BackendId();
  EXPECT_FALSE(suggestion_generator()->BackendIdToInternalId(backend_id));

  // Test that internal ID retrieval with valid backend IDs works correctly.
  std::string valid_guid_digits = "00000000-0000-0000-0000-000000000000";
  for (int i = 1; i <= 2; i++) {
    valid_guid_digits.back() = base::NumberToString(i)[0];
    backend_id = Suggestion::BackendId(valid_guid_digits);

    // Check that querying AutofillSuggestionGenerator::BackendIdToInternalId(~)
    // with a new backend id creates a new entry in the
    // |backend_to_internal_map_| and |internal_to_backend_map_| maps.
    const InternalId& internal_id =
        suggestion_generator()->BackendIdToInternalId(backend_id);
    EXPECT_TRUE(internal_id);
    EXPECT_EQ(static_cast<int>(
                  suggestion_generator()->backend_to_internal_map_.size()),
              i);
    EXPECT_EQ(static_cast<int>(
                  suggestion_generator()->internal_to_backend_map_.size()),
              i);

    // Check that querying AutofillSuggestionGenerator::BackendIdToInternalId(~)
    // again returns the previously added entry, and does not create a new entry
    // in the |backend_to_internal_map_| and |internal_to_backend_map_| maps.
    EXPECT_TRUE(suggestion_generator()->BackendIdToInternalId(backend_id) ==
                internal_id);
    EXPECT_EQ(static_cast<int>(
                  suggestion_generator()->backend_to_internal_map_.size()),
              i);
    EXPECT_EQ(static_cast<int>(
                  suggestion_generator()->internal_to_backend_map_.size()),
              i);
  }

  // The test cases below are run after the
  // AutofillSuggestionGenerator::BackendIdToInternalId(~) test cases to ensure
  // the maps |backend_to_internal_map_| and |internal_to_backend_map_| are
  // populated.

  // Test that backend ID retrieval with an invalid internal ID works correctly.
  EXPECT_TRUE(
      suggestion_generator()->InternalIdToBackendId(InternalId())->empty());

  // Test that backend ID retrieval with valid internal IDs works correctly.
  for (int i = 1; i <= 2; i++) {
    backend_id = suggestion_generator()->InternalIdToBackendId(InternalId(i));
    EXPECT_FALSE(backend_id->empty());
    valid_guid_digits.back() = base::NumberToString(i)[0];
    EXPECT_EQ(*backend_id, valid_guid_digits);
  }
}

}  // namespace autofill
