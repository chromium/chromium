// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"

#include <variant>
#include <vector>

#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

namespace {

using testing::Field;
using testing::Matcher;

// Custom matcher to verify the icon image expectations.
MATCHER_P2(SuggestionIconHasImageOrUrl, expected_image, expected_url, "") {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    auto* custom_icon_url =
        std::get_if<Suggestion::CustomIconUrl>(&arg.custom_icon);
    GURL url = custom_icon_url ? **custom_icon_url : GURL();
    return url == expected_url;
  } else {
    CHECK(std::holds_alternative<gfx::Image>(arg.custom_icon));
    return gfx::test::AreImagesEqual(std::get<gfx::Image>(arg.custom_icon),
                                     expected_image);
  }
}

Matcher<Suggestion> EqualsLoyaltyCardSuggestion(
    const std::u16string& number,
    const std::u16string& merchant_name,
    const std::string& id) {
  return EqualsSuggestion(
      SuggestionType::kLoyaltyCardEntry, number,
      /*is_main_text_primary=*/true, Suggestion::Icon::kNoIcon,
      {{Suggestion::Text(merchant_name)}}, Suggestion::Guid(id));
}

#if !BUILDFLAG(IS_ANDROID)
Matcher<Suggestion> EqualsLoyaltyCardSuggestion(
    const std::u16string& number,
    const std::u16string& merchant_name,
    const std::string& id,
    Suggestion::LetterMonochromeIcon letter_icon) {
  return AllOf(EqualsLoyaltyCardSuggestion(number, merchant_name, id),
               Field(&Suggestion::custom_icon, letter_icon));
}
#endif  // !BUILDFLAG(IS_ANDROID)

Matcher<Suggestion> EqualsManageLoyaltyCardsSuggestion() {
  return EqualsSuggestion(
      SuggestionType::kManageLoyaltyCard,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
      Suggestion::Icon::kSettings);
}

class ValuableSuggestionGeneratorTest : public testing::Test {
 public:
  ValuableSuggestionGeneratorTest() = default;

  TestValuablesDataManager& valuables_data_manager() {
    return valuables_data_manager_;
  }

  void SetUp() override {
    const std::vector<LoyaltyCard> loyalty_cards = {
        LoyaltyCard(
            /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
            /*merchant_name=*/"CVS Pharmacy",
            /*program_name=*/"CVS Extra",
            /*program_logo=*/GURL("https://empty.url.com"),
            /*loyalty_card_number=*/"987654321987654321",
            {GURL("https://domain1.example"),
             GURL("https://common-domain.example")}),
        LoyaltyCard(/*loyalty_card_id=*/ValuableId("loyalty_card_id_3"),
                    /*merchant_name=*/"Walgreens",
                    /*program_name=*/"CustomerCard",
                    /*program_logo=*/GURL("https://empty.url.com"),
                    /*loyalty_card_number=*/"998766823",
                    {GURL("https://domain2.example"),
                     GURL("https://common-domain.example")}),
        LoyaltyCard(/*loyalty_card_id=*/ValuableId("loyalty_card_id_2"),
                    /*merchant_name=*/"Ticket Maester",
                    /*program_name=*/"TourLoyal",
                    /*program_logo=*/GURL("https://empty.url.com"),
                    /*loyalty_card_number=*/"37262999281",
                    {GURL("https://domain2.example"),
                     GURL("https://common-domain.example")})};
    test_api(valuables_data_manager()).SetLoyaltyCards(loyalty_cards);
  }

  gfx::Image CustomIconForTest() { return gfx::test::CreateImage(32, 32); }

 private:
  TestValuablesDataManager valuables_data_manager_;
};

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_NoMatchingDomain) {
  EXPECT_THAT(
      GetSuggestionsForLoyaltyCards(
          valuables_data_manager(),
          GURL("https://not-existing-domain.example/test"),
          /*trigger_field_is_autofilled=*/false),
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManageLoyaltyCardsSuggestion()));
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_NoMatchingDomainAndFieldAutofilled) {
  EXPECT_THAT(
      GetSuggestionsForLoyaltyCards(
          valuables_data_manager(),
          GURL("https://not-existing-domain.example/test"),
          /*trigger_field_is_autofilled=*/true),
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kUndoOrClear),
          EqualsManageLoyaltyCardsSuggestion()));
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_WithMatchingDomain) {
  std::vector<Suggestion> suggestions_with_matching_domain =
      GetSuggestionsForLoyaltyCards(valuables_data_manager(),
                                    GURL("https://domain2.example/test"),
                                    /*trigger_field_is_autofilled=*/false);
  EXPECT_THAT(
      suggestions_with_matching_domain,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3"),
#if BUILDFLAG(IS_ANDROID)
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
#else
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kAllLoyaltyCardsEntry,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_LOYALTY_CARDS_ALL_YOUR_CARDS_SUBMENU_TITLE)),
#endif  // BUILDFLAG(IS_ANDROID)
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManageLoyaltyCardsSuggestion()));

#if !BUILDFLAG(IS_ANDROID)
  const Suggestion& lc_submenu_suggestion = suggestions_with_matching_domain[3];
  EXPECT_EQ(lc_submenu_suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_THAT(
      lc_submenu_suggestion.children,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3")));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_THAT(suggestions_with_matching_domain.back(),
              HasTrailingIcon(Suggestion::Icon::kGoogleWallet));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_WithMatchingDomainAndFieldAutofilled) {
  std::vector<Suggestion> suggestions_with_matching_domain =
      GetSuggestionsForLoyaltyCards(valuables_data_manager(),
                                    GURL("https://domain2.example/test"),
                                    /*trigger_field_is_autofilled=*/true);
  EXPECT_THAT(
      suggestions_with_matching_domain,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3"),
#if BUILDFLAG(IS_ANDROID)
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
#else
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kAllLoyaltyCardsEntry,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_LOYALTY_CARDS_ALL_YOUR_CARDS_SUBMENU_TITLE)),
#endif  // BUILDFLAG(IS_ANDROID)
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kUndoOrClear),
          EqualsManageLoyaltyCardsSuggestion()));

#if !BUILDFLAG(IS_ANDROID)
  const Suggestion& lc_submenu_suggestion = suggestions_with_matching_domain[3];
  EXPECT_EQ(lc_submenu_suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_THAT(
      lc_submenu_suggestion.children,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3")));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_THAT(suggestions_with_matching_domain.back(),
              HasTrailingIcon(Suggestion::Icon::kGoogleWallet));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_AllMatchDomain) {
  EXPECT_THAT(
      GetSuggestionsForLoyaltyCards(valuables_data_manager(),
                                    GURL("https://common-domain.example/test"),
                                    /*trigger_field_is_autofilled=*/false),
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2"),
          EqualsLoyaltyCardSuggestion(u"998766823", u"Walgreens",
                                      "loyalty_card_id_3"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManageLoyaltyCardsSuggestion()));
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_SuggestionsCustomIcon) {
  test_api(valuables_data_manager()).ClearLoyaltyCards();
  const GURL program_logo = GURL("https://empty.url.com");
  gfx::Image fake_image = CustomIconForTest();
  test_api(valuables_data_manager())
      .AddLoyaltyCard(LoyaltyCard(
          /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
          /*merchant_name=*/"CVS Pharmacy",
          /*program_name=*/"CVS Extra",
          /*program_logo=*/program_logo,
          /*loyalty_card_number=*/"987654321987654321",
          {GURL("https://domain1.example")}));
  valuables_data_manager().CacheImage(program_logo, fake_image);
  test_api(valuables_data_manager()).NotifyObservers();

  std::vector<Suggestion> suggestions = GetSuggestionsForLoyaltyCards(
      valuables_data_manager(), GURL("https://common-domain.example/test"),
      /*trigger_field_is_autofilled=*/false);

  EXPECT_THAT(suggestions,
              testing::ElementsAre(EqualsLoyaltyCardSuggestion(
                                       u"987654321987654321", u"CVS Pharmacy",
                                       "loyalty_card_id_1"),
                                   EqualsSuggestion(SuggestionType::kSeparator),
                                   EqualsManageLoyaltyCardsSuggestion()));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_THAT(suggestions.back(),
              HasTrailingIcon(Suggestion::Icon::kGoogleWallet));
#endif
  // Verify that for loyalty cards, the custom icon is shown.
  EXPECT_THAT(suggestions[0],
              SuggestionIconHasImageOrUrl(fake_image, program_logo));
}

TEST_F(ValuableSuggestionGeneratorTest,
       ExtendEmailSuggestionsWithLoyaltyCardSuggestions_ExistingLoyaltyCards) {
  const std::vector<LoyaltyCard> loyalty_cards = {
      LoyaltyCard(
          /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
          /*merchant_name=*/"CVS Pharmacy",
          /*program_name=*/"CVS Extra",
          /*program_logo=*/GURL("https://empty.url.com"),
          /*loyalty_card_number=*/"987654321987654321",
          {GURL("https://domain1.example"),
           GURL("https://common-domain.example")}),
      LoyaltyCard(/*loyalty_card_id=*/ValuableId("loyalty_card_id_2"),
                  /*merchant_name=*/"Ticket Maester",
                  /*program_name=*/"TourLoyal",
                  /*program_logo=*/GURL("https://empty.url.com"),
                  /*loyalty_card_number=*/"37262999281",
                  {GURL("https://domain2.example"),
                   GURL("https://common-matching-domain.example")})};
  test_api(valuables_data_manager()).SetLoyaltyCards(loyalty_cards);
  std::vector<Suggestion> email_suggestions = {
      Suggestion(u"test-email1@domain1.example", SuggestionType::kAddressEntry),
      Suggestion(u"test-email2@domain2.example", SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
                 SuggestionType::kManageAddress)};

  ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
      valuables_data_manager(),
      GURL("https://common-matching-domain.example/test"),
      /*trigger_field_is_autofilled=*/false, email_suggestions);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(
      email_suggestions,
      testing::ElementsAre(
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email1@domain1.example"),
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email2@domain2.example"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kManageAddress,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES)),
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2")));
#else
  EXPECT_THAT(
      email_suggestions,
      testing::ElementsAre(
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email1@domain1.example"),
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email2@domain2.example"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kAllLoyaltyCardsEntry,
                           l10n_util::GetStringUTF16(
                               IDS_AUTOFILL_LOYALTY_CARDS_SUBMENU_TITLE)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kManageAddress,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES))));
  const Suggestion& lc_submenu_suggestion = email_suggestions[3];
  EXPECT_EQ(lc_submenu_suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_THAT(
      lc_submenu_suggestion.children,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2",
                                      Suggestion::LetterMonochromeIcon(u"T")),
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1",
                                      Suggestion::LetterMonochromeIcon(u"C")),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManageLoyaltyCardsSuggestion()));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_THAT(lc_submenu_suggestion,
              HasIcon(Suggestion::Icon::kGoogleWalletMonochrome));
  EXPECT_THAT(lc_submenu_suggestion.children.back(),
              HasTrailingIcon(Suggestion::Icon::kGoogleWallet));
#endif
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(ValuableSuggestionGeneratorTest,
       ExtendEmailSuggestionsWithLoyaltyCardSuggestions_NoLoyaltyCards) {
  test_api(valuables_data_manager()).SetLoyaltyCards({});
  std::vector<Suggestion> email_suggestions = {
      Suggestion(u"test-email1@domain1.example", SuggestionType::kAddressEntry),
      Suggestion(u"test-email2@domain2.example", SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
                 SuggestionType::kManageAddress)};

  ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
      valuables_data_manager(), GURL("https://common-domain.example/test"),
      /*trigger_field_is_autofilled=*/false, email_suggestions);

  EXPECT_THAT(email_suggestions,
              testing::ElementsAre(
                  EqualsSuggestion(SuggestionType::kAddressEntry,
                                   u"test-email1@domain1.example"),
                  EqualsSuggestion(SuggestionType::kAddressEntry,
                                   u"test-email2@domain2.example"),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAddress,
                                   l10n_util::GetStringUTF16(
                                       IDS_AUTOFILL_MANAGE_ADDRESSES))));
}

TEST_F(ValuableSuggestionGeneratorTest,
       ExtendEmailSuggestionsWithLoyaltyCardSuggestions_Autofilled) {
  const std::vector<LoyaltyCard> loyalty_cards = {
      LoyaltyCard(
          /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
          /*merchant_name=*/"CVS Pharmacy",
          /*program_name=*/"CVS Extra",
          /*program_logo=*/GURL("https://empty.url.com"),
          /*loyalty_card_number=*/"987654321987654321",
          {GURL("https://domain1.example"),
           GURL("https://common-domain.example")}),
      LoyaltyCard(/*loyalty_card_id=*/ValuableId("loyalty_card_id_2"),
                  /*merchant_name=*/"Ticket Maester",
                  /*program_name=*/"TourLoyal",
                  /*program_logo=*/GURL("https://empty.url.com"),
                  /*loyalty_card_number=*/"37262999281",
                  {GURL("https://domain2.example"),
                   GURL("https://common-matching-domain.example")})};
  test_api(valuables_data_manager()).SetLoyaltyCards(loyalty_cards);

  std::vector<Suggestion> email_suggestions = {
      Suggestion(u"test-email1@domain1.example", SuggestionType::kAddressEntry),
      Suggestion(u"test-email2@domain2.example", SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM),
                 SuggestionType::kUndoOrClear),
      Suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
                 SuggestionType::kManageAddress)};

  ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
      valuables_data_manager(), GURL("https://common-domain.example/test"),
      /*trigger_field_is_autofilled=*/true, email_suggestions);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(
      email_suggestions,
      testing::ElementsAre(
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email1@domain1.example"),
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email2@domain2.example"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kUndoOrClear,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM)),
          EqualsSuggestion(
              SuggestionType::kManageAddress,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES)),
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1"),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2")));
#else
  EXPECT_THAT(
      email_suggestions,
      testing::ElementsAre(
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email1@domain1.example"),
          EqualsSuggestion(SuggestionType::kAddressEntry,
                           u"test-email2@domain2.example"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kAllLoyaltyCardsEntry,
                           l10n_util::GetStringUTF16(
                               IDS_AUTOFILL_LOYALTY_CARDS_SUBMENU_TITLE)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(
              SuggestionType::kUndoOrClear,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM)),
          EqualsSuggestion(
              SuggestionType::kManageAddress,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES))));
  const Suggestion& lc_submenu_suggestion = email_suggestions[3];
  EXPECT_EQ(lc_submenu_suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_THAT(
      lc_submenu_suggestion.children,
      testing::ElementsAre(
          EqualsLoyaltyCardSuggestion(u"987654321987654321", u"CVS Pharmacy",
                                      "loyalty_card_id_1",
                                      Suggestion::LetterMonochromeIcon(u"C")),
          EqualsLoyaltyCardSuggestion(u"37262999281", u"Ticket Maester",
                                      "loyalty_card_id_2",
                                      Suggestion::LetterMonochromeIcon(u"T")),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManageLoyaltyCardsSuggestion()));
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetSuggestionsForLoyaltyCards_SuggestionsIPH) {
  test_api(valuables_data_manager()).ClearLoyaltyCards();
  test_api(valuables_data_manager())
      .AddLoyaltyCard(LoyaltyCard(
          /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
          /*merchant_name=*/"CVS Pharmacy",
          /*program_name=*/"CVS Extra",
          /*program_logo=*/GURL("https://empty.url.com"),
          /*loyalty_card_number=*/"987654321987654321",
          {GURL("https://domain1.example")}));

  raw_ptr<const base::Feature> kIphFeature =
      &feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature;
  EXPECT_THAT(
      GetSuggestionsForLoyaltyCards(valuables_data_manager(),
                                    GURL("https://common-domain.example/test"),
                                    /*trigger_field_is_autofilled=*/false),
      testing::ElementsAre(HasIphFeature(kIphFeature), HasNoIphFeature(),
                           HasNoIphFeature()));
}
}  // namespace
}  // namespace autofill
