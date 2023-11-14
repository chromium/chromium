// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"

class FakeAutocompleteScoringModelService
    : public AutocompleteScoringModelService {
 public:
  FakeAutocompleteScoringModelService()
      : AutocompleteScoringModelService(/*model_provider=*/nullptr) {}
  // AutocompleteScoringModelService:
  void ScoreAutocompleteUrlMatch(base::CancelableTaskTracker* tracker,
                                 const ScoringSignals& scoring_signals,
                                 const std::string& match_destination_url,
                                 ResultCallback result_callback) override {
    // TODO(crbug/1405555): Properly stub this function.
  }

  void BatchScoreAutocompleteUrlMatches(
      base::CancelableTaskTracker* tracker,
      const std::vector<const ScoringSignals*>& batch_scoring_signals,
      const std::vector<std::string>& stripped_destination_urls,
      BatchResultCallback batch_result_callback) override {
    // TODO(crbug/1405555): Properly stub this function.
  }
};
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

// Used to construct the ML input matches and ML output response.
struct MlMatchTestData {
  std::string name;
  AutocompleteMatchType::Type type;
  bool allowed_to_be_default_match;
  bool shortcut_boosted;
  int traditional_relevance;
  float ml_output;

  static MlMatchTestData MakeSearch(std::string name,
                                    bool allowed_to_be_default_match,
                                    int traditional_relevance) {
    return {name,
            AutocompleteMatchType::SEARCH_SUGGEST,
            allowed_to_be_default_match,
            false,
            traditional_relevance,
            -1};
  }

  static MlMatchTestData MakeHistory(std::string name,
                                     bool allowed_to_be_default_match,
                                     int traditional_relevance,
                                     float ml_output) {
    return {name,
            AutocompleteMatchType::HISTORY_URL,
            allowed_to_be_default_match,
            false,
            traditional_relevance,
            ml_output};
  }

  static MlMatchTestData MakeShortcut(std::string name,
                                      int traditional_relevance,
                                      float ml_output) {
    return {name,
            AutocompleteMatchType::HISTORY_URL,
            true,
            true,
            traditional_relevance,
            ml_output};
  }
};
}  // namespace

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() = default;

  void SetUp() override {
    auto provider_client = std::make_unique<FakeAutocompleteProviderClient>();

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    provider_client->set_scoring_model_service(
        std::make_unique<FakeAutocompleteScoringModelService>());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

    controller_ = std::make_unique<AutocompleteController>(
        std::move(provider_client), 0, false);
  }

  void set_autocomplete_matches(std::vector<AutocompleteMatch>& matches) {
    controller_->internal_result_.Reset();
    controller_->internal_result_.AppendMatches(matches);
  }

  void MaybeRemoveCompanyEntityImages() {
    controller_->MaybeRemoveCompanyEntityImages(&controller_->internal_result_);
  }

  bool ImageURLAndImageDominantColorIsEmpty(size_t index) {
    return controller_->internal_result_.match_at(index)
               ->image_url.is_empty() &&
           controller_->internal_result_.match_at(index)
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

  AutocompleteMatch CreateSearchSuggestion() {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST;
    match.contents = u"text";
    return match;
  }

  AutocompleteProviderClient* provider_client() {
    return controller_->autocomplete_provider_client();
  }

  std::vector<std::string> MlRank(std::vector<MlMatchTestData> datas) {
    ACMatches matches;
    std::vector<AutocompleteScoringModelService::Result> ml_results;
    for (const auto& data : datas) {
      AutocompleteMatch match{nullptr, data.traditional_relevance, false,
                              data.type};
      match.shortcut_boosted = data.shortcut_boosted;
      match.allowed_to_be_default_match = data.allowed_to_be_default_match;
      match.stripped_destination_url = GURL{"https://google.com/" + data.name};
      match.contents = base::UTF8ToUTF16(data.name);
      if (data.ml_output >= 0) {
        match.scoring_signals = {{}};
        ml_results.push_back(
            {data.ml_output, match.stripped_destination_url.spec()});
      }
      matches.push_back(match);
    }

    controller_->internal_result_.Reset();
    controller_->internal_result_.AppendMatches(matches);
    base::RunLoop ml_rank_loop;
    controller_->OnUrlScoringModelDone(
        {}, base::BindLambdaForTesting([&]() {
          AutocompleteInput input(u"text", 4, metrics::OmniboxEventProto::OTHER,
                                  TestSchemeClassifier());
          controller_->internal_result_.SortAndCull(
              input, nullptr,
              provider_client()->GetOmniboxTriggeredFeatureService());
          ml_rank_loop.Quit();
        }),
        ml_results);
    ml_rank_loop.Run();

    std::vector<std::string> names;
    for (const auto& match : controller_->internal_result_)
      names.push_back(base::UTF16ToUTF8(match.contents));
    return names;
  }

 protected:
  std::unique_ptr<AutocompleteController> controller_;

  base::test::TaskEnvironment task_environment_;
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
  matches.push_back(CreateSearchSuggestion());

  set_autocomplete_matches(matches);
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
  matches.push_back(CreateSearchSuggestion());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  set_autocomplete_matches(matches);
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
  matches.push_back(CreateSearchSuggestion());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  set_autocomplete_matches(matches);
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
  matches.push_back(CreateSearchSuggestion());

  set_autocomplete_matches(matches);
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
  matches.push_back(CreateSearchSuggestion());
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));

  set_autocomplete_matches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));
  EXPECT_TRUE(
      provider_client()
          ->GetOmniboxTriggeredFeatureService()
          ->GetFeatureTriggeredInSession(
              metrics::OmniboxEventProto_Feature_COMPANY_ENTITY_ADJUSTMENT));
}

// Android and iOS aren't ready for ML and won't pass this test because they
// have their own grouping code.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, MlRanking) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::ShortcutBoosting::kShortcutBoost,
      {
          {"ShortcutBoostNonTopHitThreshold", "2"},
          {"ShortcutBoostGroupWithSearches", "true"},
      });

  EXPECT_THAT(MlRank({}), testing::ElementsAre());

  // Even if ML ranks a URL 0, it should still use traditional scores.
  EXPECT_THAT(MlRank({
                  MlMatchTestData::MakeHistory("history", true, 1400, 0),
                  MlMatchTestData::MakeSearch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "history",
                  "search",
              }));

  // Simple case of redistributing ranking among only URLs.
  EXPECT_THAT(
      MlRank({
          MlMatchTestData::MakeHistory("history 1350 .5", true, 1350, .5),
          MlMatchTestData::MakeSearch("search 1400", false, 1400),
          MlMatchTestData::MakeSearch("search 800", true, 800),
          MlMatchTestData::MakeSearch("search 600", false, 600),
          MlMatchTestData::MakeHistory("history 1200 .9", true, 1200, .9),
          MlMatchTestData::MakeHistory("history 1100 .1", false, 1100, .1),
          MlMatchTestData::MakeHistory("history 500 .2", true, 500, .2),
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
      MlRank({
          MlMatchTestData::MakeHistory("history 1400 .5", true, 1400, .5),
          MlMatchTestData::MakeSearch("search", true, 1300),
          MlMatchTestData::MakeHistory("history 1200 1", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 1",
          "search",
          "history 1400 .5",
      }));

  // Can change the default from search to history.
  EXPECT_THAT(
      MlRank({
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeHistory("history 1400 .5", false, 1400, .5),
          MlMatchTestData::MakeHistory("history 1200 1", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 1",
          "search 1300",
          "history 1400 .5",
      }));

  // Can change the default from history to search.
  EXPECT_THAT(
      MlRank({
          MlMatchTestData::MakeHistory("history 1400 .5", true, 1400, .5),
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeHistory("history 1200 1", false, 1200, .9),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1200 1",
          "history 1400 .5",
      }));

  // Can redistribute shortcut boosting to non-shortcuts.
  EXPECT_THAT(
      MlRank({
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeShortcut("shortcut 1000 .1", 1000, .1),
          MlMatchTestData::MakeSearch("search 1200", true, 1200),
          MlMatchTestData::MakeHistory("history 1400 .9", false, 1400, .9),
          MlMatchTestData::MakeHistory("history 1100 .5", true, 1100, .5),
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
      MlRank({
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeShortcut("shortcut 1000 .1", 1000, .1),
          MlMatchTestData::MakeSearch("search 1200", true, 1200),
          MlMatchTestData::MakeHistory("history 1400 .5", false, 1400, .5),
          MlMatchTestData::MakeHistory("history 1100 .9", true, 1100, .9),
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
      MlRank({
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeShortcut("shortcut 1000 .7", 1000, .7),
          MlMatchTestData::MakeSearch("search 1200", true, 1200),
          MlMatchTestData::MakeHistory("history 1400 .5", false, 1400, .5),
          MlMatchTestData::MakeHistory("history 1350 .2", false, 1350, .2),
          MlMatchTestData::MakeHistory("history 1100 .8", true, 1100, .8),
          MlMatchTestData::MakeHistory("history 1050 .9", false, 1050, .9),
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
      MlRank({
          MlMatchTestData::MakeShortcut("shortcut 1450 .7", 1450, .7),
          MlMatchTestData::MakeSearch("search 1200", true, 1200),
          MlMatchTestData::MakeHistory("history 1400 .9", false, 1400, .9),
      }),
      testing::ElementsAreArray({
          "shortcut 1450 .7",
          "history 1400 .9",
          "search 1200",
      }));

  // Can decrease the number of URLs above searches.
  EXPECT_THAT(
      MlRank({
          MlMatchTestData::MakeHistory("history 1400 .5", true, 1400, .5),
          MlMatchTestData::MakeShortcut("shortcut 1000 .1", 1000, .1),
          MlMatchTestData::MakeSearch("search 1300", true, 1300),
          MlMatchTestData::MakeSearch("search 1200", true, 1200),
          MlMatchTestData::MakeHistory("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1400 .5",
          "shortcut 1000 .1",
      }));
}
#endif  //  BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) &&
        //  !BUILDFLAG(IS_IOS)
