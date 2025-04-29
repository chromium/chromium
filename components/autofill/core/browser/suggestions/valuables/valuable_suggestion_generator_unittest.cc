// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/valuables/valuable_suggestion_generator.h"

#include <vector>

#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class ValuableSuggestionGeneratorTest : public testing::Test {
 public:
  ValuableSuggestionGeneratorTest() = default;

  TestValuablesDataManager& valuables_data_manager() {
    return valuables_data_manager_;
  }

  gfx::Image CustomIconForTest() { return gfx::test::CreateImage(32, 32); }

 private:
  TestValuablesDataManager valuables_data_manager_;
};

TEST_F(ValuableSuggestionGeneratorTest,
       GetLoyaltyCardSuggestions_ReturnMatchingSuggestions) {
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

  const Matcher<Suggestion> lc1_suggestion_matcher =
      EqualsSuggestion(SuggestionType::kLoyaltyCardEntry, u"987654321987654321",
                       /*is_main_text_primary=*/true, Suggestion::Icon::kNoIcon,
                       {{Suggestion::Text(u"CVS Pharmacy")}},
                       Suggestion::Guid("loyalty_card_id_1"));
  const Matcher<Suggestion> lc2_suggestion_matcher =
      EqualsSuggestion(SuggestionType::kLoyaltyCardEntry, u"998766823",
                       /*is_main_text_primary=*/true, Suggestion::Icon::kNoIcon,
                       {{Suggestion::Text(u"Walgreens")}},
                       Suggestion::Guid("loyalty_card_id_3"));
  const Matcher<Suggestion> lc3_suggestion_matcher =
      EqualsSuggestion(SuggestionType::kLoyaltyCardEntry, u"37262999281",
                       /*is_main_text_primary=*/true, Suggestion::Icon::kNoIcon,
                       {{Suggestion::Text(u"Ticket Maester")}},
                       Suggestion::Guid("loyalty_card_id_2"));

  const Matcher<Suggestion> separatorMatcher =
      EqualsSuggestion(SuggestionType::kSeparator);
  const Matcher<Suggestion> manageLoyaltyCardsMatcher =
      EqualsSuggestion(SuggestionType::kManageLoyaltyCard,
                       u"Manage loyalty cards...", Suggestion::Icon::kSettings);

  EXPECT_THAT(
      GetLoyaltyCardSuggestions(
          valuables_data_manager(),
          GURL("https://not-existing-domain.example/test")),
      testing::ElementsAre(lc1_suggestion_matcher, lc3_suggestion_matcher,
                           lc2_suggestion_matcher, separatorMatcher,
                           manageLoyaltyCardsMatcher));
  EXPECT_THAT(
      GetLoyaltyCardSuggestions(valuables_data_manager(),
                                GURL("https://domain2.example/test")),
      testing::ElementsAre(lc3_suggestion_matcher, lc2_suggestion_matcher,
                           separatorMatcher, lc1_suggestion_matcher,
                           separatorMatcher, manageLoyaltyCardsMatcher));

  EXPECT_THAT(
      GetLoyaltyCardSuggestions(valuables_data_manager(),
                                GURL("https://common-domain.example/test")),
      testing::ElementsAre(lc1_suggestion_matcher, lc3_suggestion_matcher,
                           lc2_suggestion_matcher, separatorMatcher,
                           manageLoyaltyCardsMatcher));
}

TEST_F(ValuableSuggestionGeneratorTest,
       GetLoyaltyCardSuggestions_SuggestionsCustomIcon) {
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

  std::vector<Suggestion> suggestions = GetLoyaltyCardSuggestions(
      valuables_data_manager(), GURL("https://common-domain.example/test"));

  EXPECT_THAT(suggestions,
              testing::ElementsAre(
                  EqualsSuggestion(
                      SuggestionType::kLoyaltyCardEntry, u"987654321987654321",
                      /*is_main_text_primary=*/true, Suggestion::Icon::kNoIcon,
                      {{Suggestion::Text(u"CVS Pharmacy")}},
                      Suggestion::Guid("loyalty_card_id_1")),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageLoyaltyCard,
                                   u"Manage loyalty cards...",
                                   Suggestion::Icon::kSettings)));

  // Verify that for loyalty cards, the custom icon is shown.
  EXPECT_THAT(suggestions[0],
              SuggestionIconHasImageOrUrl(fake_image, program_logo));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ValuableSuggestionGeneratorTest,
       GetLoyaltyCardSuggestions_SuggestionsIPH) {
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
      GetLoyaltyCardSuggestions(valuables_data_manager(),
                                GURL("https://common-domain.example/test")),
      testing::ElementsAre(HasIphFeature(kIphFeature), HasNoIphFeature(),
                           HasNoIphFeature()));
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace
}  // namespace autofill
