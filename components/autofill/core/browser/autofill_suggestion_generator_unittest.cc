// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
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
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
  AutofillSuggestionGeneratorTest() = default;

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
  }

  AutofillSuggestionGenerator* suggestion_generator() {
    return suggestion_generator_.get();
  }

  TestPersonalDataManager* personal_data() { return &personal_data_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
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
  EXPECT_EQ(absl::get<std::string>(virtual_card_suggestion.payload),
            "00000000-0000-0000-0000-000000000001");

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "");

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(absl::get<std::string>(real_card_suggestion.payload),
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
  EXPECT_EQ(absl::get<std::string>(virtual_card_suggestion.payload),
            "00000000-0000-0000-0000-000000000001");

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, AutofillType(CREDIT_CARD_NUMBER),
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/false,
          "");

  EXPECT_EQ(real_card_suggestion.frontend_id, 0);
  EXPECT_EQ(absl::get<std::string>(real_card_suggestion.payload),
            "00000000-0000-0000-0000-000000000002");
  EXPECT_TRUE(real_card_suggestion.custom_icon.IsEmpty());
}

// Credit card name field suggestion with metadata for virtual cards in Autofill
// popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_VirtualCardNameField) {
  scoped_feature_list_.InitAndEnableFeature(
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
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true,
          "");

  // "Virtual card" text is prefixed to the name.
  EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
            u"Virtual card");
  EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"Mojo Jojo");

#if BUILDFLAG(IS_ANDROID)
  // For Android, the label is "Network ....1234".
  EXPECT_EQ(virtual_card_name_field_suggestion.label,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
#elif BUILDFLAG(IS_IOS)
  // For IOS, the label is "....1234".
  EXPECT_EQ(virtual_card_name_field_suggestion.label,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Network ....1234, expires on mm/yy".
  EXPECT_EQ(
      virtual_card_name_field_suggestion.label,
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
  scoped_feature_list_.InitAndEnableFeature(
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
          /*prefix_matched_suggestion=*/false, /*virtual_card_option=*/true,
          "");

  // Only card number is displayed on the first line.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value, u"");

  // "Virtual card" is the label.
  EXPECT_EQ(virtual_card_number_field_suggestion.label, u"Virtual card");
}

// Credit card name field suggestion with metadata for non-virtual cards in
// Autofill popup.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateCreditCardSuggestion_PopupWithMetadata_NonVirtualCardNameField) {
  scoped_feature_list_.InitAndEnableFeature(
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
          "");

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Mojo Jojo");
  EXPECT_EQ(real_card_name_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_ANDROID)
  // For Android, the label is "Network ....1234".
  EXPECT_EQ(real_card_name_field_suggestion.label,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
#elif BUILDFLAG(IS_IOS)
  // For IOS, the label is "....1234".
  EXPECT_EQ(real_card_name_field_suggestion.label,
            internal::GetObfuscatedStringForCardDigits(u"1111", 4));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Network ....1234, expires on mm/yy".
  EXPECT_EQ(
      real_card_name_field_suggestion.label,
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
  scoped_feature_list_.InitAndEnableFeature(
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
          "");

  // Only the card number is displayed on the first line.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value,
            base::StrCat({u"Visa  ", internal::GetObfuscatedStringForCardDigits(
                                         u"1111", 4)}));
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // For mobile devices, the label is the expiration date formatted as mm/yy.
  EXPECT_EQ(
      real_card_number_field_suggestion.label,
      base::StrCat({u"04/", base::UTF8ToUTF16(test::NextYear().substr(2))}));
#else
  // For Desktop, the label is the descriptive expiration date formatted as
  // "Expires on mm/yy".
  EXPECT_EQ(real_card_number_field_suggestion.label,
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
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(
      &server_card, form_structure));
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(
      &local_card, form_structure));

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

TEST_F(AutofillSuggestionGeneratorTest, GetIBANSuggestions) {
  std::vector<IBAN*> ibans;

  IBAN iban0(base::GenerateGUID());
  iban0.set_value(u"CH56 0483 5012 3456 7800 9");
  iban0.set_nickname(u"My doctor's IBAN");
  ibans.push_back(&iban0);

  IBAN iban1(base::GenerateGUID());
  iban1.set_value(u"DE91 1000 0000 0123 4567 89");
  iban1.set_nickname(u"My brother's IBAN");
  ibans.push_back(&iban1);

  IBAN iban2(base::GenerateGUID());
  iban2.set_value(u"GR96 0810 0010 0000 0123 4567 890");
  iban2.set_nickname(u"My teacher's IBAN");
  ibans.push_back(&iban2);

  IBAN iban3(base::GenerateGUID());
  iban3.set_value(u"PK70 BANK 0000 1234 5678 9000");
  ibans.push_back(&iban3);

  std::vector<Suggestion> iban_suggestions =
      AutofillSuggestionGenerator::GetSuggestionsForIBANs(ibans);
  EXPECT_TRUE(iban_suggestions.size() == 4);

  EXPECT_EQ(iban_suggestions[0].main_text.value,
            u"CH" + iban0.RepeatEllipsisForTesting(4) + u"9");
  EXPECT_EQ(iban_suggestions[0].label, u"My doctor's IBAN");
  EXPECT_EQ(iban_suggestions[0].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[1].main_text.value,
            u"DE" + iban1.RepeatEllipsisForTesting(4) + u"89");
  EXPECT_EQ(iban_suggestions[1].label, u"My brother's IBAN");
  EXPECT_EQ(iban_suggestions[1].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[2].main_text.value,
            u"GR" + iban2.RepeatEllipsisForTesting(5) + u"890");
  EXPECT_EQ(iban_suggestions[2].label, u"My teacher's IBAN");
  EXPECT_EQ(iban_suggestions[2].frontend_id, POPUP_ITEM_ID_IBAN_ENTRY);

  EXPECT_EQ(iban_suggestions[3].main_text.value,
            u"PK" + iban3.RepeatEllipsisForTesting(4) + u"9000");
  EXPECT_EQ(iban_suggestions[3].label, u"");
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
  EXPECT_EQ(promo_code_suggestions[0].label, u"test_value_prop_text_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<std::string>(), "1");
  EXPECT_EQ(promo_code_suggestions[0].frontend_id,
            POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].label, u"test_value_prop_text_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<std::string>(), "2");
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
  EXPECT_EQ(promo_code_suggestions[0].label, u"test_value_prop_text_1");
  EXPECT_FALSE(
      absl::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].frontend_id,
            POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);
}

}  // namespace autofill
