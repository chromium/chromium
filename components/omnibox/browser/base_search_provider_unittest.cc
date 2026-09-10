// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace {

SearchSuggestionParser::SuggestResult BuildSuggestion(
    const std::u16string& query,
    AutocompleteMatchType::Type type,
    omnibox::SuggestType suggest_type,
    std::vector<int> subtypes,
    const std::string& additional_query_params,
    int relevance,
    bool should_prerender) {
  std::optional<omnibox::SuggestTemplateInfo> suggest_template_info;
  if (!additional_query_params.empty()) {
    suggest_template_info.emplace();
    base::StringPairs kv_pairs;
    base::SplitStringIntoKeyValuePairs(additional_query_params, '=', '&',
                                       &kv_pairs);
    for (const auto& pair : kv_pairs) {
      (*suggest_template_info
            ->mutable_default_search_parameters())[pair.first] = pair.second;
    }
  }

  return SearchSuggestionParser::SuggestResult(
      /*suggestion=*/query,
      /*type=*/type,
      /*suggest_type=*/suggest_type,
      /*subtypes=*/subtypes,
      /*match_contents=*/query,
      /*match_contents_prefix=*/u"",
      /*annotation=*/std::u16string(),
      /*deletion_url=*/std::string(),
      /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/relevance,
      /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*should_prerender=*/should_prerender,
      /*input_text=*/query, suggest_template_info);
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
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"weather los angeles";

  omnibox::RichAnswerTemplate answer_template;

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

  // Ensure answers are not copied over existing answers.
  map.clear();
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_HISTORY, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300,
      /*relevance_from_server=*/true,
      /*input_text=*/query);
  omnibox::RichAnswerTemplate answer_template2;
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
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, PreserveImageWhenDeduplicating) {
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"wrist wa";
  omnibox::SuggestTemplateInfo entity_info;
  entity_info.mutable_image()->set_url("https://picsum.photos/200");

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
  less_relevant.SetSuggestTemplateInfo(entity_info);
  provider_->AddMatchToMap(
      less_relevant, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());

  AutocompleteMatch match = map.begin()->second;
  EXPECT_EQ(entity_info.image().url(), match.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(omnibox::TYPE_NATIVE_CHROME, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];
  EXPECT_EQ(entity_info.image().url(), duplicate.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);

  // Ensure images are not copied over existing images.
  map.clear();
  omnibox::SuggestTemplateInfo entity_info2;
  entity_info2.mutable_image()->set_url("https://picsum.photos/300");
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      omnibox::TYPE_CATEGORICAL_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_LOW,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetSuggestTemplateInfo(entity_info2);
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
  EXPECT_EQ(entity_info2.image().url(), match.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, match.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, match.suggest_type);
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  duplicate = match.duplicate_matches[0];
  EXPECT_EQ(entity_info.image().url(), duplicate.image_url.spec());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, PreserveSubtypesWhenDeduplicating) {
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
  EXPECT_TRUE(match.subtypes.contains(omnibox::SUBTYPE_PERSONAL));
  EXPECT_TRUE(match.subtypes.contains(omnibox::SUBTYPE_TRENDS));
  EXPECT_EQ(1300, match.relevance);

  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, duplicate.type);
  EXPECT_EQ(omnibox::TYPE_CATEGORICAL_QUERY, duplicate.suggest_type);
  ASSERT_EQ(1U, duplicate.subtypes.size());
  EXPECT_TRUE(duplicate.subtypes.contains(omnibox::SUBTYPE_TRENDS));
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
  using TemplateAction = omnibox::SuggestTemplateInfo::TemplateAction;
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
      // TemplateAction action_uri and search_params:
      "", {}, {}},

    { "no change: supplied url, no search params",
      "https://www.google.com",
      // TemplateAction action_uri and search_params:
      "https://maps.google.com", {}, {}},

    // Cases meant to generate new URL:
    // - action_uri has to be empty,
    // - search_params have to be non-empty.
    { "generate: single query param",
      "https://g.co",
      // TemplateAction action_uri and search_params:
      "", {{"a", "3"}}, {"a=3"}},

    { "generate: multiple query params",
      "https://g.co:119/search?q=a#f",
      // TemplateAction action_uri and search_params:
      "", {{"a", "3"}, {"A", "7"}},
        {"A=7&a=3", "a=3&A=7"}},
      // clang-format on
  };

  for (const auto& test_case : test_cases) {
    TemplateAction template_action;
    template_action.set_action_uri(test_case.action_url);
    for (const auto& param : test_case.search_params) {
      template_action.mutable_search_parameters()->insert(
          {param.first, param.second});
    }

    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.additional_query_params = "never=used&shouldnt=be";
    SearchTermsData search_terms_data;
    TemplateURLData template_url_data;
    template_url_data.SetURL(test_case.base_url);
    auto template_url = std::make_unique<TemplateURL>(template_url_data);

    auto action = BaseSearchProvider::CreateActionInSuggest(
        std::move(template_action), template_url->url_ref(), search_terms_args,
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
TEST_F(BaseSearchProviderTest, CreateActionInSuggest_SchemeValidation) {
  using TemplateAction = omnibox::SuggestTemplateInfo::TemplateAction;

  struct {
    const char* test_name;
    TemplateAction::ActionType action_type;
    const char* action_url;
    bool expect_valid;
  } test_cases[]{
      {"CALL: tel scheme is valid", TemplateAction::CALL, "tel:123456", true},
      {"CALL: HTTP scheme is invalid", TemplateAction::CALL,
       "http://example.com", false},
      {"CALL: HTTPS scheme is invalid", TemplateAction::CALL,
       "https://example.com", false},
      {"CALL: chrome scheme is invalid", TemplateAction::CALL,
       "chrome://settings", false},
      {"DIRECTIONS: HTTP scheme is valid", TemplateAction::DIRECTIONS,
       "http://example.com", true},
      {"DIRECTIONS: HTTPS scheme is valid", TemplateAction::DIRECTIONS,
       "https://example.com", true},
      {"DIRECTIONS: tel scheme is invalid", TemplateAction::DIRECTIONS,
       "tel:123456", false},
      {"DIRECTIONS: chrome scheme is invalid", TemplateAction::DIRECTIONS,
       "chrome://settings", false},
      {"REVIEWS: HTTP scheme is valid", TemplateAction::REVIEWS,
       "http://example.com", true},
      {"REVIEWS: HTTPS scheme is valid", TemplateAction::REVIEWS,
       "https://example.com", true},
      {"REVIEWS: tel scheme is invalid", TemplateAction::REVIEWS, "tel:123456",
       false},
      {"REVIEWS: chrome scheme is invalid", TemplateAction::REVIEWS,
       "chrome://settings", false},
  };

  for (const auto& test_case : test_cases) {
    TemplateAction template_action;
    template_action.set_action_type(test_case.action_type);
    template_action.set_action_uri(test_case.action_url);

    TemplateURLRef::SearchTermsArgs search_terms_args;
    SearchTermsData search_terms_data;
    TemplateURLData template_url_data;
    template_url_data.SetURL("https://www.google.com");
    auto template_url = std::make_unique<TemplateURL>(template_url_data);

    auto action = BaseSearchProvider::CreateActionInSuggest(
        std::move(template_action), template_url->url_ref(), search_terms_args,
        search_terms_data);

    EXPECT_EQ(action != nullptr, test_case.expect_valid)
        << "while evaluating case `" << test_case.test_name << "`";
  }
}

TEST_F(BaseSearchProviderTest, SuggestTemplateInfoPopulatesMatch) {
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"Washington Wizards";

  // TODO(crbug.com/417745802): Update to check if actions get populated
  // correctly.
  omnibox::SuggestTemplateInfo suggest_template_info;
  suggest_template_info.set_style(omnibox::SuggestTemplateInfo::DEFAULT);
  suggest_template_info.set_type_icon(
      omnibox::SuggestTemplateInfo_IconType_SEARCH_LOOP_WITH_SPARKLE);
  suggest_template_info.mutable_primary_text()->set_text("Washington Wizards");
  suggest_template_info.mutable_secondary_text()->set_text("MIA");
  omnibox::SuggestTemplateInfo::Image* image =
      suggest_template_info.mutable_image();
  image->set_url("http://example.com/a.png");
  image->set_dominant_color("#233875");
  image->set_type(omnibox::SuggestTemplateInfo::Image::TYPE_LARGE);
  (*suggest_template_info.mutable_default_search_parameters())["gs_ssp"] =
      "abc";

  SearchSuggestionParser::SuggestResult result(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  result.SetSuggestTemplateInfo(suggest_template_info);
  provider_->AddMatchToMap(
      result, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  // Match should be populated using the SuggestTemplateInfo instead of the
  // empty EntityInfo proto. Match fields like contents (primary text) and
  // description (secondary text) are updated in `SearchSuggestionParser` so
  // will not be shown as updated here.
  ASSERT_EQ(1U, map.size());
  AutocompleteMatch match = map.begin()->second;
  EXPECT_EQ(suggest_template_info.image().dominant_color(),
            match.image_dominant_color);
  EXPECT_EQ(omnibox::SuggestTemplateInfo::DEFAULT,
            match.suggest_template->style());
}

TEST_F(BaseSearchProviderTest, SuggestTemplateInfoRichImagePopulatesMatch) {
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"cute cat";

  omnibox::SuggestTemplateInfo suggest_template_info;
  suggest_template_info.set_style(omnibox::SuggestTemplateInfo::RICH_IMAGE);
  suggest_template_info.mutable_image()->set_url("http://example.com/cat.png");

  SearchSuggestionParser::SuggestResult result(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_NATIVE_CHROME,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  result.SetSuggestTemplateInfo(suggest_template_info);
  provider_->AddMatchToMap(
      result, AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  const AutocompleteMatch& match = map.begin()->second;
  ASSERT_TRUE(match.suggest_template.has_value());
  EXPECT_EQ(omnibox::SuggestTemplateInfo::RICH_IMAGE,
            match.suggest_template->style());
  EXPECT_EQ("http://example.com/cat.png", match.image_url.spec());
}

TEST_F(BaseSearchProviderTest, AnswerAndImageOnlyPopulatedForGoogle) {
  std::u16string query = u"weather";
  omnibox::RichAnswerTemplate answer_template;

  omnibox::SuggestTemplateInfo entity_info;
  entity_info.mutable_image()->set_url("https://example.com/image.png");
  entity_info.mutable_image()->set_dominant_color("#ffffff");

  SearchSuggestionParser::SuggestResult result(
      query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_QUERY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  result.SetRichAnswerTemplate(answer_template);
  result.SetSuggestTemplateInfo(entity_info);

  // 1. Non-Google search engine: fields should NOT be populated.
  {
    TemplateURLData non_google_data;
    non_google_data.SetURL("https://evil.com/search?q={searchTerms}");
    auto non_google_turl = std::make_unique<TemplateURL>(non_google_data);
    TestBaseSearchProvider::MatchMap map;
    provider_->AddMatchToMap(
        result, AutocompleteInput(), non_google_turl.get(),
        client_->GetTemplateURLService()->search_terms_data(),
        TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
    ASSERT_EQ(1U, map.size());
    EXPECT_FALSE(map.begin()->second.answer_template.has_value());
    EXPECT_TRUE(map.begin()->second.image_url.is_empty());
  }

  // 2. Google search engine: fields SHOULD be populated.
  {
    TemplateURLData google_data;
    google_data.SetURL("https://www.google.com/search?q={searchTerms}");
    auto google_turl = std::make_unique<TemplateURL>(google_data);
    TestBaseSearchProvider::MatchMap map;
    provider_->AddMatchToMap(
        result, AutocompleteInput(), google_turl.get(),
        client_->GetTemplateURLService()->search_terms_data(),
        TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
    ASSERT_EQ(1U, map.size());
    EXPECT_TRUE(map.begin()->second.answer_template.has_value());
    EXPECT_EQ("https://example.com/image.png",
              map.begin()->second.image_url.spec());
  }
}

TEST_F(BaseSearchProviderTest, EntityImageMustBeHostedBySearchEngine) {
  // A search engine may supply an entity image that it hosts itself: while it
  // is in use it already observes what is typed into the omnibox, so fetching
  // such an image tells it nothing new. Naming a host it does not control is a
  // different matter, and stays blocked.
  struct {
    const char* search_url;
    const char* image_url;
    bool expected_allowed;
  } kCases[] = {
      // Images are commonly served from a dedicated subdomain, so the
      // comparison is by registrable domain rather than by origin.
      {"https://search.brave.com/search?q={searchTerms}",
       "https://imgs.search.brave.com/a.png", true},
      {"https://search.brave.com/search?q={searchTerms}",
       "https://search.brave.com/a.png", true},
      // A different site, however it is dressed up.
      {"https://search.brave.com/search?q={searchTerms}",
       "https://evil.com/a.png", false},
      {"https://search.brave.com/search?q={searchTerms}",
       "https://brave.com.evil.com/a.png", false},
      // Not https. Requests like the second one are the reason for the check.
      {"https://search.brave.com/search?q={searchTerms}",
       "http://search.brave.com/a.png", false},
      {"https://search.brave.com/search?q={searchTerms}",
       "http://192.168.0.1/a.png", false},
      {"https://search.brave.com/search?q={searchTerms}", "not a url", false},
      {"https://search.brave.com/search?q={searchTerms}", "", false},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.image_url);

    omnibox::SuggestTemplateInfo suggest_template_info;
    suggest_template_info.mutable_image()->set_url(test_case.image_url);
    suggest_template_info.mutable_image()->set_dominant_color("#ffffff");

    std::u16string query = u"weather";
    SearchSuggestionParser::SuggestResult result(
        query, AutocompleteMatchType::SEARCH_SUGGEST, omnibox::TYPE_QUERY,
        /*subtypes=*/{}, /*from_keyword=*/false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
        /*relevance=*/1300, /*relevance_from_server=*/true,
        /*input_text=*/query);
    result.SetSuggestTemplateInfo(suggest_template_info);

    TemplateURLData data;
    data.SetURL(test_case.search_url);
    auto turl = std::make_unique<TemplateURL>(data);

    TestBaseSearchProvider::MatchMap map;
    provider_->AddMatchToMap(
        result, AutocompleteInput(), turl.get(),
        client_->GetTemplateURLService()->search_terms_data(),
        TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
    ASSERT_EQ(1U, map.size());
    const AutocompleteMatch& match = map.begin()->second;

    EXPECT_EQ(test_case.expected_allowed, !match.image_url.is_empty());
    if (test_case.expected_allowed) {
      EXPECT_EQ(test_case.image_url, match.image_url.spec());
      EXPECT_EQ("#ffffff", match.image_dominant_color);
    } else {
      EXPECT_TRUE(match.image_dominant_color.empty());
    }
    // Answers remain Google-only.
    EXPECT_FALSE(match.answer_template.has_value());
  }
}
