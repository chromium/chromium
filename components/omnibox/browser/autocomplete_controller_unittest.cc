// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
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
