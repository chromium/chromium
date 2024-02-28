// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() : controller_(&task_environment_) {}

  void SetAutocompleteMatches(const std::vector<AutocompleteMatch>& matches) {
    controller_.internal_result_.ClearMatches();
    controller_.internal_result_.AppendMatches(matches);
  }

  void MaybeRemoveCompanyEntityImages() {
    controller_.MaybeRemoveCompanyEntityImages(&controller_.internal_result_);
  }

  bool ImageURLAndImageDominantColorIsEmpty(size_t index) {
    return controller_.internal_result_.match_at(index)->image_url.is_empty() &&
           controller_.internal_result_.match_at(index)
               ->image_dominant_color.empty();
  }

  AutocompleteMatch CreateHistoryURLMatch(std::string destination_url) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::Type::HISTORY_URL;
    match.destination_url = GURL(destination_url);
    return match;
  }

  AutocompleteMatch CreateCompanyEntityMatch(std::string website_uri) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY;
    match.website_uri = website_uri;
    match.image_url = GURL("https://url");
    match.image_dominant_color = "#000000";
    return match;
  }

  AutocompleteMatch CreateSearchMatch(std::u16string contents = u"text") {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST;
    match.contents = contents;
    return match;
  }

  AutocompleteMatch CreateStarterPackMatch(std::u16string keyword) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::Type::STARTER_PACK;
    match.contents = keyword;
    match.keyword = keyword;
    match.associated_keyword = std::make_unique<AutocompleteMatch>(
        nullptr, 1000, false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
    match.associated_keyword->keyword = keyword;
    return match;
  }

  AutocompleteMatch CreateSearchMatch(std::string name,
                                      bool allowed_to_be_default_match,
                                      int traditional_relevance) {
    auto match =
        CreateAutocompleteMatch(name, AutocompleteMatchType::SEARCH_SUGGEST,
                                allowed_to_be_default_match, false,
                                traditional_relevance, std::nullopt);
    match.keyword = u"keyword";
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(name));
    return match;
  }

  AutocompleteMatch CreatePersonalizedZeroPrefixMatch(
      std::string name,
      int traditional_relevance) {
    auto match = CreateAutocompleteMatch(
        name, AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED, false, false,
        traditional_relevance, std::nullopt);
    match.keyword = u"keyword";
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(std::u16string());
    match.suggestion_group_id = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
    match.subtypes.emplace(omnibox::SUBTYPE_PERSONAL);
    match.subtypes.emplace(omnibox::SUBTYPE_ZERO_PREFIX);
    return match;
  }

  AutocompleteMatch CreateHistoryUrlMlScoredMatch(
      std::string name,
      bool allowed_to_be_default_match,
      int traditional_relevance,
      float ml_output) {
    return CreateAutocompleteMatch(name, AutocompleteMatchType::HISTORY_URL,
                                   allowed_to_be_default_match, false,
                                   traditional_relevance, ml_output);
  }

  AutocompleteMatch CreateBoostedShortcutMatch(std::string name,
                                               int traditional_relevance,
                                               float ml_output) {
    return CreateAutocompleteMatch(name, AutocompleteMatchType::HISTORY_URL,
                                   true, true, traditional_relevance,
                                   ml_output);
  }

  AutocompleteMatch CreateKeywordHintMatch(std::string name,
                                           int traditional_relevance) {
    auto match = CreateAutocompleteMatch(
        name, AutocompleteMatchType::SEARCH_SUGGEST, false, false,
        traditional_relevance, std::nullopt);
    match.keyword = u"keyword";
    match.associated_keyword = std::make_unique<AutocompleteMatch>(
        nullptr, 1000, false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
    return match;
  }

  AutocompleteMatch CreateHistoryClusterMatch(std::string name,
                                              int traditional_relevance) {
    return CreateAutocompleteMatch(name, AutocompleteMatchType::HISTORY_CLUSTER,
                                   false, false, traditional_relevance,
                                   std::nullopt);
  }

  AutocompleteMatch CreateAutocompleteMatch(std::string name,
                                            AutocompleteMatchType::Type type,
                                            bool allowed_to_be_default_match,
                                            bool shortcut_boosted,
                                            int traditional_relevance,
                                            std::optional<float> ml_output) {
    AutocompleteMatch match{nullptr, traditional_relevance, false, type};
    match.shortcut_boosted = shortcut_boosted;
    match.allowed_to_be_default_match = allowed_to_be_default_match;
    match.destination_url = GURL{"https://google.com/" + name};
    match.stripped_destination_url = GURL{"https://google.com/" + name};
    match.contents = base::UTF8ToUTF16(name);
    match.contents_class = {{0, 1}};
    if (ml_output.has_value()) {
      match.scoring_signals = {{}};
      match.scoring_signals->set_site_engagement(ml_output.value());
    }
    return match;
  }

  FakeAutocompleteProviderClient* provider_client() {
    return static_cast<FakeAutocompleteProviderClient*>(
        controller_.autocomplete_provider_client());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeAutocompleteController controller_;
};

TEST_F(AutocompleteControllerTest, RemoveCompanyEntityImage_LeastAggressive) {
  // Set feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kCompanyEntityIconAdjustment,
      {{OmniboxFieldTrial::kCompanyEntityIconAdjustmentGroup.name,
        "least-aggressive"}});
  std::vector<AutocompleteMatch> matches;
  // In the least aggressive experiment group the historical match must be the
  // first match and the company entity must be the second match to replace the
  // entity's image.
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));
  EXPECT_TRUE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

TEST_F(AutocompleteControllerTest,
       CompanyEntityImageNotRemoved_LeastAggressive) {
  // Set feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kCompanyEntityIconAdjustment,
      {{OmniboxFieldTrial::kCompanyEntityIconAdjustmentGroup.name,
        "least-aggressive"}});
  std::vector<AutocompleteMatch> matches;
  // Entity is not the second suggestion. Entity's image should not be removed.
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));

  MaybeRemoveCompanyEntityImages();
  // The entity's image_url should remain as is.
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));
  EXPECT_FALSE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

TEST_F(AutocompleteControllerTest, RemoveCompanyEntityImage_Moderate) {
  // Set feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kCompanyEntityIconAdjustment,
      {{OmniboxFieldTrial::kCompanyEntityIconAdjustmentGroup.name,
        "moderate"}});
  std::vector<AutocompleteMatch> matches;
  // In the moderate experiment group the historical match must be the first
  // match and the company entity can be in any slot.
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));
  EXPECT_TRUE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

TEST_F(AutocompleteControllerTest, CompanyEntityImageNotRemoved_Moderate) {
  // Set feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kCompanyEntityIconAdjustment,
      {{OmniboxFieldTrial::kCompanyEntityIconAdjustmentGroup.name,
        "moderate"}});
  std::vector<AutocompleteMatch> matches;
  // History match is not the first suggestion. Entity's image should not be
  // removed.
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));

  MaybeRemoveCompanyEntityImages();
  // The entity's image_url should remain as is.
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));
  EXPECT_FALSE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

TEST_F(AutocompleteControllerTest, RemoveCompanyEntityImage_MostAggressive) {
  // Set feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kCompanyEntityIconAdjustment,
      {{OmniboxFieldTrial::kCompanyEntityIconAdjustmentGroup.name,
        "most-aggressive"}});
  std::vector<AutocompleteMatch> matches;
  // In the most aggressive experiment group both the historical match and
  // company entity can be in any slot.
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));
  EXPECT_TRUE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

TEST_F(AutocompleteControllerTest, FilterMatchesForInstantKeywordWithBareAt) {
  base::test::ScopedFeatureList feature_list(
      omnibox::kOmniboxKeywordModeRefresh);

  SetAutocompleteMatches({
      CreateSearchMatch(u"@"),
      CreateCompanyEntityMatch("https://example.com"),
      CreateHistoryURLMatch("https://example.com"),
      CreateStarterPackMatch(u"@bookmarks"),
      CreateStarterPackMatch(u"@history"),
      CreateStarterPackMatch(u"@tabs"),
  });

  AutocompleteInput input(u"@", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.MaybeCleanSuggestionsForKeywordMode(
      input, &controller_.internal_result_);

  EXPECT_EQ(controller_.internal_result_.size(), 4u);
  EXPECT_TRUE(
      std::all_of(controller_.internal_result_.begin(),
                  controller_.internal_result_.end(), [](const auto& match) {
                    return match.type == AutocompleteMatchType::STARTER_PACK ||
                           match.contents == u"@";
                  }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_SyncAnd2Async) {
  auto sync_match = CreateSearchMatch("sync", true, 1300);
  auto async_match1 = CreateSearchMatch("async_1", true, 1200);
  auto async_match2 = CreateSearchMatch("async_2", true, 1250);
  {
    SCOPED_TRACE("Sync pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {sync_match}),
                testing::ElementsAreArray({
                    "sync",
                }));
  }
  {
    SCOPED_TRACE("1st async pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, false, {sync_match, async_match1}),
                testing::ElementsAreArray({
                    "sync",
                    "async_1",
                }));
  }
  {
    SCOPED_TRACE(
        "Last async pass. Verify the correct matches are shown ranked by "
        "relevance.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, true, {async_match1, async_match2}),
                testing::ElementsAreArray({
                    "async_2",
                    "async_1",
                }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_TransferringOldMatches) {
  auto pass1_match1 = CreateSearchMatch("pass1_match1", true, 1300);
  auto pass1_match2 = CreateSearchMatch("pass1_match2", true, 1200);
  auto pass1_match3 = CreateSearchMatch("pass1_match3", true, 1100);
  auto pass2_match1 = CreateSearchMatch("pass2_match1", true, 1000);
  auto pass3_match2 = CreateSearchMatch("pass3_match2", true, 900);
  auto pass3_match3 = CreateSearchMatch("pass3_match3", true, 800);
  auto pass3_match4 = CreateSearchMatch("pass3_match4", true, 700);
  auto pass4_match1 = CreateSearchMatch("pass4_match1", true, 600);
  auto pass5_match1 = CreateSearchMatch("pass5_match1", true, 500);
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  true, false, {pass1_match1, pass1_match2, pass1_match3}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match2",
                  "pass1_match3",
              }));
  // # of matches decreased from 3 to 2. So 1 match should be transferred. The
  // lowest ranked match should be transferred. It should keep its score and be
  // ranked above the new non-transferred match.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass1_match1, pass2_match1}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match3",
                  "pass2_match1",
              }));
  // # of matches remained 3. So no matches should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass3_match2, pass3_match3, pass3_match4}),
              testing::ElementsAreArray({
                  "pass3_match2",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Transferred matches should not be allowed to be default.
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(false, false, {pass4_match1}),
      testing::ElementsAreArray({
          "pass4_match1",
          "pass3_match3",
          "pass3_match4",
      }));
  // Lowest ranked match should be transferred. But old matches still present
  // shouldn't count, and the next lowest match should be transferred.
  // Transferred match scores should be capped to the new default, therefore,
  // the transferred `pass3_match3` should be demoted to last even though it
  // originally outscored `pass3_match4`.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass4_match1, pass3_match4}),
              testing::ElementsAreArray({
                  "pass4_match1",
                  "pass3_match4",
                  "pass3_match3",
              }));
  // Sync updates should also transfer old matches. Lowest ranked, not
  // necessarily lowest scored, match should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {pass5_match1}),
              testing::ElementsAreArray({
                  "pass5_match1",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Expire updates should not transfer old matches.
  EXPECT_THAT(controller_.SimulateExpirePass(), testing::ElementsAreArray({
                                                    "pass5_match1",
                                                }));
  // Async updates after the expire update should transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(false, false, {}),
              testing::ElementsAreArray({
                  "pass5_match1",
              }));
  // The last async pass shouldn't transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {}),
              testing::ElementsAreArray<std::string>({}));
}

TEST_F(AutocompleteControllerTest, UpdateResult_PreservingDefault) {
  auto match1 = CreateSearchMatch("match1", true, 100);
  auto match2 = CreateSearchMatch("match2", true, 200);
  auto match3 = CreateSearchMatch("match3", true, 300);
  auto match4 = CreateSearchMatch("match4", true, 400);
  auto match5 = CreateSearchMatch("match5", true, 500);
  auto match6 = CreateSearchMatch("match6", true, 600);
  auto match7 = CreateSearchMatch("match7", true, 700);
  auto match8 = CreateSearchMatch("match8", true, 800);
  auto match9 = CreateSearchMatch("match9", true, 900);
  {
    SCOPED_TRACE("Load a default suggestion.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {match1}),
                testing::ElementsAreArray({
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Don't preserve default on sync pass with short inputs.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    true, false, {match1, match2},
                    FakeAutocompleteController::CreateInput(u"x")),
                testing::ElementsAreArray({
                    "match2",
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with short inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match3}),
        testing::ElementsAreArray({
            "match2",
            "match3",
        }));
    // Preserve default on last async pass with short inputs. Preserve default
  }
  {
    SCOPED_TRACE("across multiple passes.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match2, match4}),
        testing::ElementsAreArray({
            "match2",
            "match4",
        }));
    // Preserve default on sync pass with long inputs. Preserve default across
  }
  {
    SCOPED_TRACE("multiple inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(true, false, {match2, match5}),
        testing::ElementsAreArray({
            "match2",
            "match5",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match6}),
        testing::ElementsAreArray({
            "match2",
            "match6",
        }));
  }
  {
    SCOPED_TRACE("Don't preserve default if it's transferred.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match7, match8}),
        testing::ElementsAreArray({
            "match8",
            "match7",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on last async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match8, match9}),
        testing::ElementsAreArray({
            "match8",
            "match9",
        }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_Ranking) {
  // Higher scored suggestions are ranked higher.
  // Clear results between each test to avoid default preserving applying.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", true, 800),
              }),
              testing::ElementsAreArray({
                  "800",
                  "500",
              }));

  // Default suggestion must be allowed to be default.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", false, 800),
              }),
              testing::ElementsAreArray({
                  "500",
                  "800",
              }));

// Android and iOS don't use the same grouping logic as desktop
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Searches should be grouped above non-shortcut-boosted URLs.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
              }),
              testing::ElementsAreArray({
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Default can be a non-search if it's scored higher than all the searches.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
                  CreateHistoryUrlMlScoredMatch("history1000", true, 1000, 1),
              }),
              testing::ElementsAreArray({
                  "history1000",
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Shortcut boosting is re-distributed when ML Scoring is enabled.  That is
  // tested in the `MlRanking` test below.
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_config;
  if (!scoped_config.GetMLConfig().ml_url_scoring) {
    // Shortcut boosted suggestions should be ranked above searches, even if
    // they're scored lower.
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                    CreateHistoryUrlMlScoredMatch("history850", true, 850, 1),
                    CreateSearchMatch("search700", true, 700),
                    CreateSearchMatch("search750", true, 750),
                    CreateBoostedShortcutMatch("shortcut600", 600, 1),
                    CreateBoostedShortcutMatch("shortcut650", 650, 1),
                }),
                testing::ElementsAreArray({
                    "history850",
                    "shortcut650",
                    "shortcut600",
                    "search750",
                    "search700",
                    "history800",
                }));
  }

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

TEST_F(AutocompleteControllerTest, UpdateResult_ZPSEnabledAndShownInSession) {
  // Populate TemplateURLService with a keyword.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Create a zero-suggest input.
  auto zps_input = FakeAutocompleteController::CreateInput(u"");
  zps_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);

  {
    SCOPED_TRACE("Zero-prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are updated in the internal result set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              2u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              2u);
  }
  {
    SCOPED_TRACE("More zero-prefix suggestions are offered asynchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/false, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                        CreatePersonalizedZeroPrefixMatch("zps_3", 1448),
                        CreatePersonalizedZeroPrefixMatch("zps_4", 1447),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                    "zps_3",
                    "zps_4",
                }));
    // If zero-prefix suggestions are offered multiple times in the session, the
    // most recent count is logged.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=false is called due to user idleness");
    controller_.Stop(/*clear_result=*/false);
    // Stop with clear_result=false does not clear the internal result set and
    // does not notify `OnResultChanged()`.
    EXPECT_FALSE(controller_.internal_result_.empty());
    EXPECT_FALSE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/true,
                    {
                        CreateSearchMatch("search_1", true, 900),
                    }),
                testing::ElementsAreArray({
                    "search_1",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=true is called due to popup closing");
    controller_.Stop(/*clear_result=*/true);
    // Stop with clear_result=true clears the internal result set and notifies
    // `OnResultChanged()`.
    EXPECT_TRUE(controller_.internal_result_.empty());
    EXPECT_TRUE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are reset in the internal result set.
    EXPECT_FALSE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
  }
}

// Android and iOS aren't ready for ML and won't pass this test because they
// have their own grouping code.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, MlRanking) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().stable_search_blending = false;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // Even if ML ranks a URL 0, it should still use traditional scores.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history", true, 1400, 0),
                  CreateSearchMatch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "history",
                  "search",
              }));

  // Simple case of redistributing ranking among only URLs.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          CreateSearchMatch("search 1400", false, 1400),
          CreateSearchMatch("search 800", true, 800),
          CreateSearchMatch("search 600", false, 600),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search 1400",
          "search 800",
          "search 600",
          "history 1350 .5",
          "history 500 .2",
          "history 1100 .1",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history A 1350 .2", true, 1350, .2),
          CreateHistoryUrlMlScoredMatch("history B 1200 .2", true, 1200, .2),
          CreateHistoryUrlMlScoredMatch("history C 1100 .2", false, 1100, .2),
          CreateHistoryUrlMlScoredMatch("history D 300 .2", true, 300, .2),
          CreateHistoryUrlMlScoredMatch("history E 200 .2", true, 200, .2),
          CreateHistoryUrlMlScoredMatch("history F 100 .2", true, 100, .2),
      }),
      testing::ElementsAreArray({
          "history A 1350 .2",
          "history B 1200 .2",
          "history C 1100 .2",
          "history D 300 .2",
          "history E 200 .2",
          "history F 100 .2",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search",
          "history 1400 .5",
      }));

  // Can change the default from search to history.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search 1300",
          "history 1400 .5",
      }));

  // Can change the default from history to search.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", false, 1200, .9),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1200 .9",
          "history 1400 .5",
      }));

  // Can redistribute shortcut boosting to non-shortcuts.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .9", false, 1400, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .9",
          "search 1200",
          "history 1100 .5",
          "shortcut 1000 .1",
      }));

  // Can 'consume' shortcut boosting by assigning it to a match that's becoming
  // default anyways.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1400 .5",
          "shortcut 1000 .1",
      }));

  // Can increase the number of URLs above searches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .7", 1000, .7),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1350 .2", false, 1350, .2),
          CreateHistoryUrlMlScoredMatch("history 1100 .8", true, 1100, .8),
          CreateHistoryUrlMlScoredMatch("history 1050 .9", false, 1050, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .8",
          "history 1050 .9",
          "search 1300",
          "search 1200",
          "shortcut 1000 .7",
          "history 1400 .5",
          "history 1350 .2",
      }));

  // Can increase the number of URLs above searches even when the default was a
  // URL.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateBoostedShortcutMatch("shortcut 1450 .7", 1450, .7),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .9", false, 1400, .9),
      }),
      testing::ElementsAreArray({
          "shortcut 1450 .7",
          "history 1400 .9",
          "search 1200",
      }));

  // Can decrease the number of URLs above searches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1300", true, 1300),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1400 .5",
          "shortcut 1000 .1",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.ClearMatches();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
              CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
          CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));
}

TEST_F(AutocompleteControllerTest, MlRanking_StableSearchRanking) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().stable_search_blending = true;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // Even if ML ranks a URL 0, it should still use traditional scores.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history", true, 1400, 0),
                  CreateSearchMatch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "history",
                  "search",
              }));

  // Simple case of redistributing ranking among only URLs.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          CreateSearchMatch("search 1400", false, 1400),
          CreateSearchMatch("search 800", true, 800),
          CreateSearchMatch("search 600", false, 600),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search 1400",
          "search 800",
          "search 600",
          "history 1350 .5",
          "history 500 .2",
          "history 1100 .1",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history B 1200 .2", true, 1200, .2),
          CreateHistoryUrlMlScoredMatch("history E 200 .2", true, 200, .2),
          CreateHistoryUrlMlScoredMatch("history A 1350 .2", true, 1350, .2),
          CreateHistoryUrlMlScoredMatch("history D 300 .2", true, 300, .2),
          CreateHistoryUrlMlScoredMatch("history C 1100 .2", false, 1100, .2),
          CreateHistoryUrlMlScoredMatch("history F 100 .2", true, 100, .2),
      }),
      testing::ElementsAreArray({
          "history A 1350 .2",
          "history B 1200 .2",
          "history C 1100 .2",
          "history D 300 .2",
          "history E 200 .2",
          "history F 100 .2",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search",
          "history 1400 .5",
      }));

  // Can not change the default from search to history.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1200 .9",
          "history 1400 .5",
      }));

  // Can not change the default from history to search.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", false, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1400 .5",
          "search 1300",
          "history 1200 .9",
      }));

  // Can redistribute shortcut boosting to non-shortcuts.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .9", false, 1400, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .9",
          "search 1200",
          "history 1100 .5",
          "shortcut 1000 .1",
      }));

  // Can not 'consume' shortcut boosting by assigning it to a match that's
  // becoming default.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1300", true, 1300),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "history 1400 .5",
          "search 1300",
          "search 1200",
          "shortcut 1000 .1",
      }));

  // Can not 'consume' shortcut boosting by leaving it to a shortcut that's
  // becoming default.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateBoostedShortcutMatch("shortcut 1000 .9", 1000, .9),
          CreateSearchMatch("search 1300", true, 1300),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
      }),
      testing::ElementsAreArray({
          "shortcut 1000 .9",
          "history 1400 .5",
          "search 1300",
          "search 1200",
          "history 1100 .1",
      }));

  // Can not redistribute a no-op boosted shortcut (i.e. a boosted shortcut that
  // was default).
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateBoostedShortcutMatch("shortcut 1400 .1", 1400, .1),
          CreateSearchMatch("search 1300", true, 1300),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
          CreateHistoryUrlMlScoredMatch("history 1000 .5", true, 1000, .5),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1000 .5",
          "shortcut 1400 .1",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.ClearMatches();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
              CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
          CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));
}

TEST_F(AutocompleteControllerTest, MlRanking_MappedSearchBlending) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().mapped_search_blending = true;

  scoped_ml_config.GetMLConfig().mapped_search_blending_min = 600;
  scoped_ml_config.GetMLConfig().mapped_search_blending_max = 2800;
  scoped_ml_config.GetMLConfig().mapped_search_blending_grouping_threshold =
      1400;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // If ML ranks a URL 0, then the final relevance score should be set to the
  // value of `mapped_search_blending_min` (since ML scores are mapped using the
  // formula "final_score = min + ml_score * (max - min))".
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history", true, 1400, 0),
                  CreateSearchMatch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "search",
                  "history",
              }));

  // Simple case of ranking with linear score mapping.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1700 (== 600 + 0.5 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          // Final score: 2580 (== 600 + 0.9 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "history 1350 .5",
          "history 500 .2",
          "history 1100 .1",
      }));

  // Verify that URLs are grouped above searches if their final score is
  // greater than `grouping_threshold` (i.e. "shortcut boosting").
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1700 (== 600 + 0.5 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          CreateSearchMatch("search 1400", false, 1400),
          CreateSearchMatch("search 800", true, 800),
          CreateSearchMatch("search 600", false, 600),
          // Final score: 2580 (== 600 + 0.9 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "history 1350 .5",
          "search 1400",
          "search 800",
          "search 600",
          "history 500 .2",
          "history 1100 .1",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      // Each of the below URL suggestions are assigned an initial relevance
      // score of 1040 (== 600 + 0.2 * (2800 - 600)). After initial assignment,
      // score adjustment logic is applied in order to generate the final
      // relevance scores (which are guaranteed to be distinct).
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1039
          CreateHistoryUrlMlScoredMatch("history B 1200 .2", true, 1200, .2),
          // Final score: 1036
          CreateHistoryUrlMlScoredMatch("history E 200 .2", true, 200, .2),
          // Final score: 1040
          CreateHistoryUrlMlScoredMatch("history A 1350 .2", true, 1350, .2),
          // Final score: 1037
          CreateHistoryUrlMlScoredMatch("history D 300 .2", true, 300, .2),
          // Final score: 1038
          CreateHistoryUrlMlScoredMatch("history C 1100 .2", false, 1100, .2),
          // Final score: 1035
          CreateHistoryUrlMlScoredMatch("history F 100 .2", true, 100, .2),
      }),
      testing::ElementsAreArray({
          "history A 1350 .2",
          "history B 1200 .2",
          "history C 1100 .2",
          "history D 300 .2",
          "history E 200 .2",
          "history F 100 .2",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", true, 1400, .2),
          CreateSearchMatch("search", true, 1100),
          // Final score: 1260 (== 600 + 0.3 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .3", true, 1200, .3),
      }),
      testing::ElementsAreArray({
          "history 1200 .3",
          "search",
          "history 1400 .2",
      }));

  // Can change the default from search to history (unlike StableSearchRanking
  // variant).
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1200", true, 1200),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", false, 1400, .2),
          // Final score: 1260 (== 600 + 0.3 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .3", true, 1100, .3),
      }),
      testing::ElementsAreArray({
          "history 1100 .3",
          "search 1200",
          "history 1400 .2",
      }));

  // Can change the default from history to search (unlike StableSearchRanking
  // variant).
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", true, 1400, .2),
          CreateSearchMatch("search 1300", true, 1300),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .1", false, 1200, .1),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .2",
          "history 1200 .1",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.Reset();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
              CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_MLRanking_PreserveDefault) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().stable_search_blending = true;

  // ML ranking should preserve search defaults.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  true, true,
                  {
                      CreateSearchMatch("search 1300", true, 1300),
                  }),
              testing::ElementsAreArray({
                  "search 1300",
              }));
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1400", true, 1400, 1),
              CreateSearchMatch("search 1300", true, 1300),
          }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400",
      }));

  // ML ranking should preserve non-search defaults.
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1300", true, 1300, 1),
          }),
      testing::ElementsAreArray({
          "history 1300",
      }));
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1500", true, 1500, 1),
              CreateSearchMatch("search 1400", true, 1400),
              CreateHistoryUrlMlScoredMatch("history 1300", true, 1300, .1),
          }),
      testing::ElementsAreArray({
          "history 1300",
          "search 1400",
          "history 1500",
      }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_MLRanking_AllMatches) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().stable_search_blending = true;

  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateHistoryUrlMlScoredMatch("history 100", true, 100, 1),
              CreateHistoryUrlMlScoredMatch("history 200", true, 200, 1),
              CreateHistoryUrlMlScoredMatch("history 300", true, 300, 1),
              CreateHistoryUrlMlScoredMatch("history 400", true, 400, 1),
              CreateHistoryUrlMlScoredMatch("history 500", true, 500, 1),
              CreateHistoryUrlMlScoredMatch("history 600", true, 600, 1),
              CreateHistoryUrlMlScoredMatch("history 700", true, 700, 1),
              CreateHistoryUrlMlScoredMatch("history 800", true, 800, 1),
              CreateHistoryUrlMlScoredMatch("history 900", true, 900, 1),
              CreateHistoryUrlMlScoredMatch("history 1000", true, 1000, 1),
          }),
      testing::ElementsAreArray({
          "history 1000",
          "history 900",
          "history 800",
          "history 700",
          "history 600",
          "history 500",
          "history 400",
          "history 300",
      }));

  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateHistoryUrlMlScoredMatch("history 100 .9", true, 100, .9),
              CreateHistoryUrlMlScoredMatch("history 200 .8", true, 200, .8),
              CreateHistoryUrlMlScoredMatch("history 300 .7", true, 300, .7),
              CreateHistoryUrlMlScoredMatch("history 400 .6", true, 400, .6),
              CreateHistoryUrlMlScoredMatch("history 500 .5", true, 500, .5),
              CreateHistoryUrlMlScoredMatch("history 600 .4", true, 600, .4),
              CreateHistoryUrlMlScoredMatch("history 700 .3", true, 700, .3),
              CreateHistoryUrlMlScoredMatch("history 800 .2", true, 800, .2),
              CreateHistoryUrlMlScoredMatch("history 900 .1", true, 900, .1),
              CreateHistoryUrlMlScoredMatch("history 1000 0", true, 1000, 0),
          }),
      testing::ElementsAreArray({
          "history 100 .9",
          "history 200 .8",
          "history 300 .7",
          "history 400 .6",
          "history 500 .5",
          "history 600 .4",
          "history 700 .3",
          "history 800 .2",
      }));
}
#endif  //  BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) &&
        //  !BUILDFLAG(IS_IOS)

TEST_F(AutocompleteControllerTest, UpdateResult_NotifyingAndTimers) {
  {
    SCOPED_TRACE("Expect immediate notification after sync pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
  }
  {
    SCOPED_TRACE("Expect debounced notification after async pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect debouncing to reset after each async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect delayed notification after expiration.");
    controller_.UpdateResult(AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kExpirePass);
  }
  {
    SCOPED_TRACE("Expect immediate notification after the last async pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass.");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect immediate notification after the last async pass, even if a "
        "debounced notification is pending.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);

    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());

    controller_.GetFakeProvider().done_ = true;
    task_environment_.FastForwardBy(base::Milliseconds(10));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass (2).");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect no stop update after a sync only pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPassOnly);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect a stop update if the async passes takes too long. Expect no "
        "notification.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    controller_.ExpectStopAfter(1500);
  }
  {
    SCOPED_TRACE(
        "Expect a stop update to flush any pending notification for completed "
        "non-final async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    for (size_t i = 0; i < 9; ++i) {
      task_environment_.FastForwardBy(base::Milliseconds(150));
      controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    }
    controller_.ExpectStopAfter(150);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect debounced expire notification.");
    controller_.GetFakeProvider().done_ = false;
    AutocompleteMatch transferred_match{
        nullptr, 1000, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED};
    transferred_match.from_previous = true;
    controller_.GetFakeProvider().matches_ = {transferred_match};
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    // Expire timer is 500ms. Debounce delay is 200ms.
    controller_.ExpectOnResultChanged(
        700, AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectStopAfter(800);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, ExplicitStop) {
  // Besides the `Stop()` fired by the timer, which is tested in
  // `UpdateResult_NotifyingAndTimers`, there's also user triggered `Stop()`s
  // tests here.

  auto matches = {CreateSearchMatch("search", true, 900)};

  {
    SCOPED_TRACE(
        "Stop with clear_result=false and no pending changes should not notify"
        "`OnResultChanged()` - there's no change to notify of.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.Stop(false);
    controller_.ExpectStopAfter(0);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with clear_result=false and pending changes should not notify"
        "`OnResultChanged()` - the last pending change should be abandoned to "
        "avoid changes as the user's e.g. down arrowing..");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.Stop(false);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectStopAfter(0);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with clear_result=true and no pending notifications should "
        "notify `OnResultChanged()` - observers should know the results were "
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(true);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with clear_result=true and pending notifications should notify "
        "`OnResultChanged()` - observers should know the results were "
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(true);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_ForceAllowedToBeDefault) {
  auto set_feature = [](bool enabled) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        omnibox_feature_configs::ForceAllowedToBeDefault::
            kForceAllowedToBeDefault,
        enabled);
    return omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ForceAllowedToBeDefault>();
  };

  {
    // When disabled, a not-defaultable history match should not be default.
    SCOPED_TRACE("Disabled");
    auto disabled_config = set_feature(false);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "search",
                    "history",
                }));
  }
  {
    // An initially not-defaultable history match can be made defaultable.
    SCOPED_TRACE("Enabled");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Initially defaultable matches should not be made non-defaultable even if
    // they don't qualify for forcing defaultable.
    SCOPED_TRACE("Enabled defaultable");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", true, 300, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Keyword matches shouldn't be made defaultable.
    SCOPED_TRACE("Enabled keyword");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateKeywordHintMatch("keyword", 1000),
                }),
                testing::ElementsAreArray({
                    "search",
                    "keyword",
                }));
  }
  {
    // Should not force default when `prevent_inline_autocomplete_` is true.
    SCOPED_TRACE("Enabled prevent inline autocomplete");
    auto enabled_config = set_feature(true);
    controller_.internal_result_.ClearMatches();
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(
            true, true,
            {
                CreateSearchMatch("search", true, 200),
                CreateAutocompleteMatch("history",
                                        AutocompleteMatchType::HISTORY_CLUSTER,
                                        false, false, 1000, std::nullopt),
            },
            FakeAutocompleteController::CreateInput(u"test", false, true)),
        testing::ElementsAreArray({
            "search",
            "history",
        }));
  }
}

TEST_F(AutocompleteControllerTest, ExtraHeaders) {
  // Populate TemplateURLService with a keyword.
  {
    TemplateURLData turl_data;
    turl_data.SetShortName(u"Keyword");
    turl_data.SetKeyword(u"keyword");
    turl_data.SetURL("https://google.com/search?q={searchTerms}");
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(turl_data));
  }

  // Populate template URL service with starter pack entries.
  for (auto& turl_data : TemplateURLStarterPackData::GetStarterPackEngines()) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl_data)));
  }

  {
    SCOPED_TRACE("@gemini starter pack match gets an extra header.");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_EQ(match.extra_headers, "X-Omnibox-Gemini:search term");
    EXPECT_EQ(match.destination_url, "https://gemini.google.com/prompt");
  }
  {
    SCOPED_TRACE("@gemini starter pack match with url override");

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kStarterPackExpansion,
        {{"StarterPackGeminiUrlOverride", "https://example.com/"}});

    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_EQ(match.extra_headers, "X-Omnibox-Gemini:search term");
    EXPECT_EQ(match.destination_url, "https://example.com/");
  }
  {
    SCOPED_TRACE("@gemini starter pack with invalid input");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term\n");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_EQ(match.extra_headers, "");
    EXPECT_EQ(match.destination_url, "https://gemini.google.com/prompt");
  }
  {
    SCOPED_TRACE("@bookmarks starter pack match does not get an extra header.");
    auto match = CreateStarterPackMatch(u"@bookmarks");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_EQ(match.extra_headers, "");
    EXPECT_EQ(match.destination_url, "chrome://bookmarks/?q=search+term");
  }
  {
    SCOPED_TRACE("search match does not get an extra header.");
    auto match = CreateSearchMatch("search term", true, 1300);

    controller_.SetMatchDestinationURL(&match);
    EXPECT_EQ(match.extra_headers, "");
    EXPECT_EQ(match.destination_url, "https://google.com/search?q=search+term");
  }
}

TEST_F(AutocompleteControllerTest, ShouldRunProvider) {
  // Disable LimitKeywordModeSuggestions flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      omnibox_feature_configs::LimitKeywordModeSuggestions::
          kLimitKeywordModeSuggestions,
      false);
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::LimitKeywordModeSuggestions>
      scoped_config;

  std::set<AutocompleteProvider::Type> expected_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with starter pack entries.
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  // Not in keyword mode, run all providers except open tab provider.
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              provider->type() != AutocompleteProvider::TYPE_OPEN_TAB)
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In keyword mode but not starter pack, LimitkeywordModeSuggestions disabled,
  // run all providers except open tab provider.
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              provider->type() != AutocompleteProvider::TYPE_OPEN_TAB)
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @tabs, run search, keyword, and open tab provider only.
  controller_.input_.UpdateText(u"@tabs", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @bookmarks, run search, keyword, and bookmarks only.
  controller_.input_.UpdateText(u"@bookmarks", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_BOOKMARK};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @history, run search, keyword, and history providers only.
  controller_.input_.UpdateText(u"@history", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_HISTORY_QUICK,
                             AutocompleteProvider::TYPE_HISTORY_URL};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

TEST_F(AutocompleteControllerTest,
       ShouldRunProvider_LimitKeywordModeSuggestions) {
  // Enable LimitKeywordModeSuggestions flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      omnibox_feature_configs::LimitKeywordModeSuggestions::
          kLimitKeywordModeSuggestions,
      true);
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::LimitKeywordModeSuggestions>
      scoped_config;

  std::set<AutocompleteProvider::Type> excluded_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with an entry for drive.google.com (to test
  // document provider) and a generic keyword entry.
  TemplateURLData drive_turl_data;
  drive_turl_data.SetShortName(u"Google Drive");
  drive_turl_data.SetKeyword(u"drive.google.com");
  drive_turl_data.SetURL("https://drive.google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(drive_turl_data));
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Test Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Not in keyword mode, run all providers except open tab provider.
  excluded_provider_types = {AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In keyword mode, all limit provider params on by default, limit document
  //  and history cluster suggestions as well.
  controller_.input_.UpdateText(u"keyword", 0, {});
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_DOCUMENT,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // For drive.google.com, run document provider.
  controller_.input_.UpdateText(u"drive.google.com", 0, {});
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // Turn off param to limit history cluster and document suggestions, ensure
  // they're run.
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::LimitKeywordModeSuggestions::
          kLimitKeywordModeSuggestions,
      {{"LimitHistoryClusterSuggestions", "false"}});
  scoped_config.Reset();
  controller_.input_.UpdateText(u"keyword", 0, {});
  excluded_provider_types = {AutocompleteProvider::TYPE_OPEN_TAB,
                             AutocompleteProvider::TYPE_DOCUMENT};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}
