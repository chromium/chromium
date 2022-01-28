// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

// TODO(crbug.com/1196021): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class AutofillSuggestionGeneratorTest : public testing::Test {
 public:
  AutofillSuggestionGeneratorTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableMerchantBoundVirtualCards);
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
  }

  AutofillSuggestionGenerator* suggestion_generator() {
    return suggestion_generator_.get();
  }

  TestPersonalDataManager* personal_data() { return &personal_data_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  std::unique_ptr<AutofillSuggestionGenerator> suggestion_generator_;
  TestAutofillClient autofill_client_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true,
          "");

  EXPECT_EQ(virtual_card_suggestion.frontend_id,
            POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);
  EXPECT_EQ(virtual_card_suggestion.backend_id,
            "00000000-0000-0000-0000-000000000001");

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "");

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(real_card_suggestion.backend_id,
            "00000000-0000-0000-0000-000000000001");
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
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true,
          "");

  EXPECT_EQ(virtual_card_suggestion.frontend_id,
            POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);
  EXPECT_EQ(virtual_card_suggestion.backend_id,
            "00000000-0000-0000-0000-000000000001");

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "");

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(real_card_suggestion.backend_id,
            "00000000-0000-0000-0000-000000000002");
  EXPECT_TRUE(real_card_suggestion.custom_icon.IsEmpty());
}

TEST_F(AutofillSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
  // Create a complete form.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  TestFormStructure form_structure(credit_card_form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < credit_card_form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure.field(i)->heuristic_type());
  }
  form_structure.SetFieldTypes(heuristic_types, server_types);

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
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(
      &server_card, form_structure));
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(
      &local_card, form_structure));

  // Reset form to reset field storage types to mock as an incomplete form.
  TestFormStructure incomplete_form_structure(credit_card_form);

  // If it is an incomplete form, it should return false;
  EXPECT_FALSE(suggestion_generator()->ShouldShowVirtualCardOption(
      &server_card, incomplete_form_structure));

  // Reset server card virtual card enrollment state.
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNSPECIFIED);
  personal_data()->ClearCreditCards();
  personal_data()->AddServerCreditCard(server_card);

  // For server card not enrolled, should return false.
  EXPECT_FALSE(suggestion_generator()->ShouldShowVirtualCardOption(
      &server_card, form_structure));
  EXPECT_FALSE(suggestion_generator()->ShouldShowVirtualCardOption(
      &local_card, form_structure));

  // Remove the server credit card.
  personal_data()->ClearCreditCards();

  // The local card no longer has a server duplicate, should return false.
  EXPECT_FALSE(suggestion_generator()->ShouldShowVirtualCardOption(
      &local_card, form_structure));
}

}  // namespace autofill
