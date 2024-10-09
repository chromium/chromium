// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

namespace {

SearchSuggestionParser::SuggestResult BuildSuggestion(
    const std::u16string& query,
    AutocompleteMatchType::Type type,
    omnibox::SuggestType suggest_type,
    std::vector<int> subtypes,
    const std::string& additional_query_params,
    int relevance,
    bool should_prerender) {
  omnibox::EntityInfo entity_info;
  entity_info.set_suggest_search_parameters(additional_query_params);

  return SearchSuggestionParser::SuggestResult(
      /*suggestion=*/query,
      /*type=*/type,
      /*suggest_type=*/suggest_type,
      /*subtypes=*/subtypes,
      /*match_contents=*/query,
      /*match_contents_prefix=*/u"",
      /*annotation=*/std::u16string(),
      /*entity_info=*/entity_info,
      /*deletion_url=*/std::string(),
      /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/relevance,
      /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*should_prerender=*/should_prerender,
      /*input_text=*/query);
}

}  // namespace

using testing::_;
using testing::NiceMock;
using testing::Return;

class TestBaseSearchProvider : public BaseSearchProvider {
 public:
  typedef BaseSearchProvider::MatchMap MatchMap;

  TestBaseSearchProvider(AutocompleteProvider::Type type,
                         AutocompleteProviderClient* client)
      : BaseSearchProvider(type, client) {}
  TestBaseSearchProvider(const TestBaseSearchProvider&) = delete;
  TestBaseSearchProvider& operator=(const TestBaseSearchProvider&) = delete;
  MOCK_CONST_METHOD1(AddProviderInfo, void(ProvidersInfo* provider_info));
  MOCK_CONST_METHOD1(ShouldAppendExtraParams,
                     bool(const SearchSuggestionParser::SuggestResult& result));
  MOCK_METHOD1(RecordDeletionResult, void(bool success));

  MOCK_METHOD2(Start,
               void(const AutocompleteInput& input, bool minimal_changes));
  using BaseSearchProvider::AddMatchToMap;

  void AddMatch(const AutocompleteMatch& match) { matches_.push_back(match); }

 protected:
  ~TestBaseSearchProvider() override = default;
};

class BaseSearchProviderTestFixture {
 protected:
  void SetUp() {
    client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(
        search_engines_test_environment_.template_url_service());

    provider_ = new NiceMock<TestBaseSearchProvider>(
        AutocompleteProvider::TYPE_SEARCH, client_.get());
  }

  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<NiceMock<TestBaseSearchProvider>> provider_;
};

class BaseSearchProviderTest : public BaseSearchProviderTestFixture,
                               public testing::Test {
 public:
  ~BaseSearchProviderTest() override = default;

  void SetUp() override { BaseSearchProviderTestFixture::SetUp(); }
};

TEST_F(BaseSearchProviderTest, PreserveAnswersWhenDeduplicating) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"weather los angeles";
  SuggestionAnswer answer;
  omnibox::AnswerType answer_type = omnibox::ANSWER_TYPE_WEATHER;

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  less_relevant.SetAnswer(answer);
  less_relevant.SetAnswerType(answer_type);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];

  EXPECT_TRUE(answer.Equals(*match.answer));
  EXPECT_EQ(answer_type, match.answer_type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  EXPECT_TRUE(answer.Equals(*duplicate.answer));
  EXPECT_EQ(answer_type, duplicate.answer_type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);

  // Ensure answers are not copied over existing answers.
  map.clear();
  SuggestionAnswer answer2;
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300,
      /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetAnswer(answer2);
  more_relevant.SetAnswerType(answer_type);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  ASSERT_EQ(1U, map.size());
  match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  duplicate = match.duplicate_matches[0];

  EXPECT_TRUE(answer2.Equals(*match.answer));
  EXPECT_EQ(answer_type, match.answer_type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  EXPECT_TRUE(answer.Equals(*duplicate.answer));
  EXPECT_EQ(answer_type, duplicate.answer_type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);
}

// Same as test above with kOmniboxSuggestionAnswerMigration enabled.
TEST_F(BaseSearchProviderTest, AnswerData_PreserveAnswersWhenDeduplicating) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"weather los angeles";

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SuggestionAnswerMigration>
      scoped_config;
  scoped_config.Get().enabled = true;

  omnibox::RichAnswerTemplate answer_template;
  answer_template.add_answers();
  answer_template.mutable_answers(0)->mutable_headline()->set_text("headline");

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  less_relevant.SetRichAnswerTemplate(answer_template);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];
  EXPECT_EQ(answer_template.answers(0).headline().text(),
            match.answer_template->answers(0).headline().text());

  // Ensure answers are not copied over existing answers.
  map.clear();
  omnibox::RichAnswerTemplate answer_template2;
  answer_template2.add_answers();
  answer_template2.mutable_answers(0)->mutable_headline()->set_text(
      "headline2");
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300,
      /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetRichAnswerTemplate(answer_template2);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  ASSERT_EQ(1U, map.size());
  match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  duplicate = match.duplicate_matches[0];

  EXPECT_EQ(answer_template2.answers(0).headline().text(),
            match.answer_template->answers(0).headline().text());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  EXPECT_EQ(answer_template.answers(0).headline().text(),
            duplicate.answer_template->answers(0).headline().text());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, PreserveImageWhenDeduplicating) {
  // Ensure categorical suggestions are enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kCategoricalSuggestions);

  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"wrist wa";
  omnibox::EntityInfo entity_info;
  entity_info.set_image_url("https://picsum.photos/200");

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      omnibox::TYPE_CATEGORICAL_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  less_relevant.SetEntityInfo(entity_info);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());

  AutocompleteMatch match = map.begin()->second;
  EXPECT_EQ(entity_info.image_url(), match.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];
  EXPECT_EQ(entity_info.image_url(), duplicate.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);

  // Ensure images are not copied over existing images.
  map.clear();
  omnibox::EntityInfo entity_info2;
  entity_info2.set_image_url("https://picsum.photos/300");
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      omnibox::TYPE_CATEGORICAL_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetEntityInfo(entity_info2);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());

  match = map.begin()->second;
  EXPECT_EQ(entity_info2.image_url(), match.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, match.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  duplicate = match.duplicate_matches[0];
  EXPECT_EQ(entity_info.image_url(), duplicate.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, PreserveSubtypesWhenDeduplicating) {
  // Ensure categorical suggestions and merging subtypes are enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kCategoricalSuggestions, omnibox::kMergeSubtypes}, {});

  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"wrist wa";

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{omnibox::SUBTYPE_PERSONAL}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      omnibox::TYPE_CATEGORICAL_QUERY,
      /*subtypes=*/{omnibox::SUBTYPE_TRENDS}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());

  AutocompleteMatch match = map.begin()->second;
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  ASSERT_EQ(2U, match.subtypes.size());
  EXPECT_TRUE(base::Contains(match.subtypes, omnibox::SUBTYPE_PERSONAL));
  EXPECT_TRUE(base::Contains(match.subtypes, omnibox::SUBTYPE_TRENDS));
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  ASSERT_EQ(1U, duplicate.subtypes.size());
  EXPECT_TRUE(base::Contains(duplicate.subtypes, omnibox::SUBTYPE_TRENDS));
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, PreserveAdditionalQueryParamsWhenDeduplicating) {
  TemplateURLData data;
  data.SetURL("http://example.com/?q={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"tom cruise";

  // Ensure that a match with empty additional query params is added to the map
  // without a pre-computed `stripped_destination_url`.
  SearchSuggestionParser::SuggestResult plain_text =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"",
                      /*relevance=*/1300, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      plain_text, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  AutocompleteMatch match = map[std::make_pair(query, "")];
  ASSERT_EQ(1300, match.relevance);
  EXPECT_EQ("http://example.com/?q=tom+cruise", match.destination_url);
  EXPECT_EQ("", match.stripped_destination_url);
  ASSERT_EQ(0U, match.duplicate_matches.size());

  // Ensure that a duplicate match, with identical search terms and an empty
  // additional query params, and with a lower relevance is added as a duplicate
  // of the existing match in the map without a pre-computed
  // `stripped_destination_url`.
  SearchSuggestionParser::SuggestResult duplicate_plain_text =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"",
                      /*relevance=*/1299, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      duplicate_plain_text, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  match = map[std::make_pair(query, "")];
  ASSERT_EQ(1300, match.relevance);
  EXPECT_EQ("http://example.com/?q=tom+cruise", match.destination_url);
  EXPECT_EQ("", match.stripped_destination_url);
  ASSERT_EQ(1U, match.duplicate_matches.size());
  ASSERT_EQ(1299, match.duplicate_matches[0].relevance);

  // Ensure that the first match, with duplicate search terms and a unique
  // non-empty additional query params, is added to the map without a
  // pre-computed `stripped_destination_url`.
  SearchSuggestionParser::SuggestResult entity_1 =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"gs_ssp=1",
                      /*relevance=*/1298, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      entity_1, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(2U, map.size());
  match = map[std::make_pair(query, "gs_ssp=1")];
  ASSERT_EQ(1298, match.relevance);
  EXPECT_EQ("http://example.com/?gs_ssp=1&q=tom+cruise", match.destination_url);
  EXPECT_EQ("", match.stripped_destination_url);
  ASSERT_EQ(0U, match.duplicate_matches.size());

  // Ensure that a subsequent match, with duplicate search terms and a unique
  // non-empty additional query params, is added to the map with a pre-computed
  // `stripped_destination_url`.
  SearchSuggestionParser::SuggestResult entity_2 =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"gs_ssp=2",
                      /*relevance=*/1297, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      entity_2, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(3U, map.size());
  match = map[std::make_pair(query, "gs_ssp=2")];
  ASSERT_EQ(1297, match.relevance);
  EXPECT_EQ("http://example.com/?gs_ssp=2&q=tom+cruise", match.destination_url);
  EXPECT_EQ(match.destination_url, match.stripped_destination_url);
  ASSERT_EQ(0U, match.duplicate_matches.size());

  // Ensure that a duplicate match, with identical search terms and additional
  // query params, and with a lower relevance is added as a duplicate of the
  // existing match in the map.
  SearchSuggestionParser::SuggestResult duplicate_1_entity_2 =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"gs_ssp=2",
                      /*relevance=*/1296, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      duplicate_1_entity_2, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(3U, map.size());
  match = map[std::make_pair(query, "gs_ssp=2")];
  ASSERT_EQ(1297, match.relevance);
  EXPECT_EQ("http://example.com/?gs_ssp=2&q=tom+cruise", match.destination_url);
  EXPECT_EQ(match.destination_url, match.stripped_destination_url);
  ASSERT_EQ(1U, match.duplicate_matches.size());
  ASSERT_EQ(1296, match.duplicate_matches[0].relevance);

  // Ensure that a duplicate match, with identical search terms and additional
  // query params, and with a higher relevance replaces the existing match in
  // the map with a pre-computed `stripped_destination_url`.
  SearchSuggestionParser::SuggestResult duplicate_2_entity_2 =
      BuildSuggestion(query, AutocompleteMatchType::SEARCH_HISTORY,
                      omnibox::TYPE_NATIVE_CHROME, {omnibox::SUBTYPE_PERSONAL},
                      /*additional_query_params=*/"gs_ssp=2",
                      /*relevance=*/1301, /*should_prerender=*/false);
  provider_->AddMatchToMap(
      duplicate_2_entity_2, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(3U, map.size());
  match = map[std::make_pair(query, "gs_ssp=2")];
  ASSERT_EQ(1301, match.relevance);
  EXPECT_EQ("http://example.com/?gs_ssp=2&q=tom+cruise", match.destination_url);
  EXPECT_EQ(match.destination_url, match.stripped_destination_url);
  ASSERT_EQ(2U, match.duplicate_matches.size());
  ASSERT_EQ(1296, match.duplicate_matches[0].relevance);
  ASSERT_EQ(1297, match.duplicate_matches[1].relevance);
}

TEST_F(BaseSearchProviderTest, MatchTailSuggestionProperly) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  AutocompleteInput autocomplete_input(
      u"weather", 7, metrics::OmniboxEventProto::BLANK, TestSchemeClassifier());

  std::u16string query = u"angeles now";
  std::u16string suggestion = u"weather los " + query;
  SearchSuggestionParser::SuggestResult suggest_result = BuildSuggestion(
      suggestion, AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
      omnibox::TYPE_TAIL, /*subtypes=*/{}, /*additional_query_params=*/"",
      /*relevance=*/1300, /*should_prerender=*/false);

  TestBaseSearchProvider::MatchMap map;
  provider_->AddMatchToMap(
      suggest_result, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1UL, map.size());
  const auto& entry = *(map.begin());
  std::string text = entry.second.GetAdditionalInfoForDebugging(
      kACMatchPropertyContentsStartIndex);
  size_t length;
  EXPECT_TRUE(base::StringToSizeT(text, &length));
  text = entry.second.GetAdditionalInfoForDebugging(
      kACMatchPropertySuggestionText);
  EXPECT_GE(text.length(), length);
}

TEST_F(BaseSearchProviderTest, DeleteDuplicateMatch) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"site.com";

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/735, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, true, false, &map);

  ASSERT_EQ(1U, map.size());
  ASSERT_TRUE(provider_->matches().empty());

  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  provider_->AddMatch(match);

  provider_->DeleteMatch(match.duplicate_matches[0]);
  ASSERT_EQ(1U, provider_->matches().size());
  ASSERT_TRUE(provider_->matches()[0].duplicate_matches.empty());
}

// Tests that the prerender hint can be aggregated to another SuggestResult.
TEST_F(BaseSearchProviderTest, PrerenderDefaultMatch) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"prerender";

  SearchSuggestionParser::SuggestResult default_suggestion(
      query, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      default_suggestion, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN,
      /*mark_as_deletable=*/false,
      /*in_keyword_mode=*/false, &map);

  SearchSuggestionParser::SuggestResult prerender_suggestion = BuildSuggestion(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_QUERY,
      /*subtypes=*/{}, /*additional_query_params=*/"",
      /*relevance=*/850, /*should_prerender=*/true);
  provider_->AddMatchToMap(
      prerender_suggestion, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN,
      /*mark_as_deletable=*/false,
      /*in_keyword_mode=*/false, &map);

  ASSERT_EQ(1U, map.size());
  ASSERT_TRUE(provider_->matches().empty());

  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  EXPECT_TRUE(BaseSearchProvider::ShouldPrerender(match));
}

class BaseSearchProviderOnDeviceSuggestionTest
    : public BaseSearchProviderTestFixture,
      public testing::TestWithParam<bool> {
 public:
  ~BaseSearchProviderOnDeviceSuggestionTest() override = default;

  void SetUp() override { BaseSearchProviderTestFixture::SetUp(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BaseSearchProviderOnDeviceSuggestionTest,
                         testing::Bool());

TEST_P(BaseSearchProviderOnDeviceSuggestionTest,
       CreateOnDeviceSearchSuggestion) {
  bool is_tail_suggestion = GetParam();
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  std::vector<std::u16string> input_texts = {
      u"googl", u"google", u"google ma", u"google map ", u"googl map login"};
  std::vector<std::u16string> suggestions = {
      u"google", u"google map", u"google map login", u"google map login",
      u"google map login"};
  std::vector<std::u16string> expected_tail_match_contents = {
      u"google", u"google map", u"map login", u"map login",
      u"google map login"};

  for (size_t i = 0; i < input_texts.size(); ++i) {
    AutocompleteInput autocomplete_input(input_texts[i],
                                         metrics::OmniboxEventProto::OTHER,
                                         TestSchemeClassifier());
    AutocompleteMatch match =
        BaseSearchProvider::CreateOnDeviceSearchSuggestion(
            provider_.get(), autocomplete_input, suggestions[i], 99,
            template_url.get(),
            client_->GetTemplateURLService()->search_terms_data(),
            TemplateURLRef::NO_SUGGESTION_CHOSEN, is_tail_suggestion);
    ASSERT_EQ(match.contents, is_tail_suggestion
                                  ? expected_tail_match_contents[i]
                                  : suggestions[i]);
    ASSERT_EQ(match.type, is_tail_suggestion
                              ? AutocompleteMatchType::SEARCH_SUGGEST_TAIL
                              : AutocompleteMatchType::SEARCH_SUGGEST);
    ASSERT_EQ(match.suggest_type,
              is_tail_suggestion ? omnibox::TYPE_TAIL : omnibox::TYPE_QUERY);
  }
}

TEST_F(BaseSearchProviderTest, CreateActionInSuggest_BuildActionURL) {
  using omnibox::ActionInfo;
  // Correlation between ActionType and UMA-recorded bucket.
  struct {
    const char* test_name;
    const char* base_url;
    const char* action_url;
    std::vector<std::pair<const char*, const char*>> search_params;
    // query params order is not guaranteed to be the same across all platforms
    // or even across multiple runs. the vector below captures possible
    // variants.
    std::vector<const char*> expect_query_params;
  } test_cases[]{
      // clang-format off
    // Cases explicitly not meant to produce any changes.
    { "no change: no supplied url, no search params",
      "https://www.google.com",
      // ActionInfo action_uri and search_params:
      "", {}, {}},

    { "no change: supplied url, no search params",
      "https://www.google.com",
      // ActionInfo action_uri and search_params:
      "https://maps.google.com", {}, {}},

    // Cases meant to generate new URL:
    // - action_uri has to be empty,
    // - search_params have to be non-empty.
    { "generate: single query param",
      "https://g.co",
      // ActionInfo action_uri and search_params:
      "", {{"a", "3"}}, {"a=3"}},

    { "generate: multiple query params",
      "https://g.co:119/search?q=a#f",
      // ActionInfo action_uri and search_params:
      "", {{"a", "3"}, {"A", "7"}},
        {"A=7&a=3", "a=3&A=7"}},
      // clang-format on
  };

  for (const auto& test_case : test_cases) {
    ActionInfo action_info;
    action_info.set_action_uri(test_case.action_url);
    for (const auto& param : test_case.search_params) {
      action_info.mutable_search_parameters()->insert(
          {param.first, param.second});
    }

    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.additional_query_params = "never=used&shouldnt=be";
    SearchTermsData search_terms_data;
    TemplateURLData template_url_data;
    template_url_data.SetURL(test_case.base_url);
    auto template_url = std::make_unique<TemplateURL>(template_url_data);

    auto action = BaseSearchProvider::CreateActionInSuggest(
        std::move(action_info), template_url->url_ref(), search_terms_args,
        search_terms_data);

    auto* action_in_suggest = OmniboxActionInSuggest::FromAction(action.get());

    // order of elements in ProtobufMap is not guaranteed, and in fact changes,
    // even within the same platform. Instead of trying to decompose the params
    // just check the params against variants that we specified in the
    // expect_query_params.
    EXPECT_EQ(action_in_suggest->search_terms_args.has_value(),
              !test_case.expect_query_params.empty())
        << "while evaluating case `" << test_case.test_name << '`';

    bool found_matching_param_sequence = test_case.expect_query_params.empty();
    for (auto* param_sequence : test_case.expect_query_params) {
      found_matching_param_sequence |=
          action_in_suggest->search_terms_args->additional_query_params ==
          param_sequence;
    }
    EXPECT_TRUE(found_matching_param_sequence)
        << "while evaluating case `" << test_case.test_name << '`';
  }
}

TEST_F(BaseSearchProviderTest, CreateAnswerAction) {
  struct {
    std::string query;
    std::vector<std::pair<std::string, std::string>> query_cgi_params;
    std::vector<std::string> possible_param_variations;
  } test_cases[]{
      // No additional params.
      {/*query=*/"Alphabet Inc Class C compare", /*query_cgi_params=*/{},
       /*possible_param_variations=*/{}},
      // One additional param.
      {/*query=*/"Alphabet Inc Class C financials",
       /*query_cgi_params=*/{{"name", "value"}},
       /*possible_param_variations=*/{"name=value"}},
      // Multiple additional params.
      {/*query=*/"About Alphabet Inc Class C",
       /*query_cgi_params=*/{{"name1", "value1"}, {"name2", "value2"}},
       /*possible_param_variations=*/
       {"name1=value1&name2=value2", "name2=value2&name1=value1"}},
  };
  omnibox::RichAnswerTemplate answer_template;
  for (const auto& test_case : test_cases) {
    omnibox::SuggestionEnhancement* enhancement =
        answer_template.mutable_enhancements()->add_enhancements();
    enhancement->set_query(test_case.query);
    for (const auto& param : test_case.query_cgi_params) {
      enhancement->mutable_query_cgi_params()->insert(
          {param.first, param.second});
    }
    TemplateURLRef::SearchTermsArgs search_terms_args;
    SearchTermsData search_terms_data;
    TemplateURLData template_url_data;
    template_url_data.SetURL("https://www.google.com/search?q={searchTerms}");
    auto template_url = std::make_unique<TemplateURL>(template_url_data);

    auto action = BaseSearchProvider::CreateAnswerAction(
        std::move(*enhancement), search_terms_args,
        omnibox::ANSWER_TYPE_FINANCE);

    auto* answer_action = OmniboxAnswerAction::FromAction(action.get());
    // Ensure search terms additional params match. Checking the exact value is
    // not easily possible as param order is not guaranteed.
    bool found_matching_param_sequence =
        test_case.possible_param_variations.empty();
    for (const std::string& param_sequence :
         test_case.possible_param_variations) {
      found_matching_param_sequence |=
          answer_action->search_terms_args.additional_query_params ==
          param_sequence;
    }
    EXPECT_TRUE(found_matching_param_sequence);
  }
}
