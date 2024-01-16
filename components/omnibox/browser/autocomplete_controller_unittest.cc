// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>
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
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() : controller_(&task_environment_) {}

  void SetAutocompleteMatches(const std::vector<AutocompleteMatch>& matches) {
    controller_.internal_result_.Reset();
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
                                traditional_relevance, absl::nullopt);
    match.keyword = u"keyword";
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
        traditional_relevance, absl::nullopt);
    match.keyword = u"keyword";
    match.associated_keyword = std::make_unique<AutocompleteMatch>(
        nullptr, 1000, false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
    return match;
  }

  AutocompleteMatch CreateHistoryClusterMatch(std::string name,
                                              int traditional_relevance) {
    return CreateAutocompleteMatch(name, AutocompleteMatchType::HISTORY_CLUSTER,
                                   false, false, traditional_relevance,
                                   absl::nullopt);
  }

  AutocompleteMatch CreateAutocompleteMatch(std::string name,
                                            AutocompleteMatchType::Type type,
                                            bool allowed_to_be_default_match,
                                            bool shortcut_boosted,
                                            int traditional_relevance,
                                            absl::optional<float> ml_output) {
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

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
    controller_.internal_result_.Reset();
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(
            true, true,
            {
                CreateSearchMatch("search", true, 200),
                CreateAutocompleteMatch("history",
                                        AutocompleteMatchType::HISTORY_CLUSTER,
                                        false, false, 1000, 1),
            },
            FakeAutocompleteController::CreateInput(u"test", false, true)),
        testing::ElementsAreArray({
            "search",
            "history",
        }));
  }
}
