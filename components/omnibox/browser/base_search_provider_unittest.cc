// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using testing::_;

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
  void AddMatchToMap(const SearchSuggestionParser::SuggestResult& result,
                     const std::string& metadata,
                     const AutocompleteInput& input,
                     const TemplateURL* template_url,
                     const SearchTermsData& search_terms_data,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     bool in_keyword_mode,
                     MatchMap* map) {
    BaseSearchProvider::AddMatchToMap(result, metadata, input, template_url,
                                      search_terms_data, accepted_suggestion,
                                      mark_as_deletable, in_keyword_mode, map);
  }

  void AddMatch(const AutocompleteMatch& match) {
    matches_.push_back(match);
  }

 protected:
  ~TestBaseSearchProvider() override {}
};

class BaseSearchProviderTest : public testing::Test {
 public:
  ~BaseSearchProviderTest() override {}

 protected:
  void SetUp() override {
    auto template_url_service = std::make_unique<TemplateURLService>(
        nullptr /* PrefService */, std::make_unique<SearchTermsData>(),
        nullptr /* KeywordWebDataService */,
        std::unique_ptr<TemplateURLServiceClient>(), base::RepeatingClosure());
    client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(std::move(template_url_service));
    provider_ = new NiceMock<TestBaseSearchProvider>(
        AutocompleteProvider::TYPE_SEARCH, client_.get());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<NiceMock<TestBaseSearchProvider>> provider_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
};

TEST_F(BaseSearchProviderTest, PreserveAnswersWhenDeduplicating) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"weather los angeles";
  SuggestionAnswer answer;
  answer.set_type(2334);

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  less_relevant.SetAnswer(answer);
  provider_->AddMatchToMap(
      less_relevant, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1U, map.size());
  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  AutocompleteMatch duplicate = match.duplicate_matches[0];

  EXPECT_TRUE(answer.Equals(*match.answer));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(1300, match.relevance);

  EXPECT_TRUE(answer.Equals(*duplicate.answer));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(850, duplicate.relevance);

  // Ensure answers are not copied over existing answers.
  map.clear();
  SuggestionAnswer answer2;
  answer2.set_type(8242);
  more_relevant = SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetAnswer(answer2);
  provider_->AddMatchToMap(
      more_relevant, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  provider_->AddMatchToMap(
      less_relevant, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);
  ASSERT_EQ(1U, map.size());
  match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  duplicate = match.duplicate_matches[0];

  EXPECT_TRUE(answer2.Equals(*match.answer));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, match.type);
  EXPECT_EQ(1300, match.relevance);

  EXPECT_TRUE(answer.Equals(*duplicate.answer));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, duplicate.type);
  EXPECT_EQ(850, duplicate.relevance);
}

TEST_F(BaseSearchProviderTest, MatchTailSuggestionProperly) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  AutocompleteInput autocomplete_input(
      u"weather", 7, metrics::OmniboxEventProto::BLANK, TestSchemeClassifier());

  std::u16string query = u"angeles now";
  std::u16string suggestion = u"weather los " + query;
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
      /*subtypes=*/{},
      /*match_contents=*/query,
      /*match_contents_prefix=*/u"...",
      /*annotation=*/std::u16string(),
      /*additional_query_params=*/std::string(),
      /*deletion_url=*/std::string(),
      /*image_dominant_color=*/std::string(),
      /*image_url=*/std::string(),
      /*from_keyword=*/false,
      /*relevance=*/1300,
      /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*should_prerender=*/false,
      /*input_text=*/query);

  TestBaseSearchProvider::MatchMap map;
  provider_->AddMatchToMap(
      suggest_result, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  ASSERT_EQ(1UL, map.size());
  const auto& entry = *(map.begin());
  std::string text =
      entry.second.GetAdditionalInfo(kACMatchPropertyContentsStartIndex);
  size_t length;
  EXPECT_TRUE(base::StringToSizeT(text, &length));
  text = entry.second.GetAdditionalInfo(kACMatchPropertySuggestionText);
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
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, std::string(), AutocompleteInput(), template_url.get(),
      client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/735, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      less_relevant, std::string(), AutocompleteInput(), template_url.get(),
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

namespace {
SearchSuggestionParser::SuggestResult BuildPrerenderSuggestion(
    std::u16string query) {
  return SearchSuggestionParser::SuggestResult(
      query, AutocompleteMatchType::SEARCH_SUGGEST,
      /*subtypes=*/{},
      /*match_contents=*/query,
      /*match_contents_prefix=*/u"...",
      /*annotation=*/std::u16string(),
      /*additional_query_params=*/std::string(),
      /*deletion_url=*/std::string(),
      /*image_dominant_color=*/std::string(),
      /*image_url=*/std::string(),
      /*from_keyword=*/false,
      /*relevance=*/850,
      /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*should_prerender=*/true,
      /*input_text=*/query);
}
}  // namespace

// Tests that the prerender hint can be aggregated to another SuggestResult.
TEST_F(BaseSearchProviderTest, PrerenderDefaultMatch) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  auto template_url = std::make_unique<TemplateURL>(data);

  TestBaseSearchProvider::MatchMap map;
  std::u16string query = u"prerender";

  SearchSuggestionParser::SuggestResult default_suggestion(
      query, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      /*subtypes=*/{}, /*from_keyword=*/false,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      default_suggestion, std::string(), AutocompleteInput(),
      template_url.get(), client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN,
      /*mark_as_deletable=*/false,
      /*in_keyword_mode=*/false, &map);

  SearchSuggestionParser::SuggestResult prerender_suggestion =
      BuildPrerenderSuggestion(query);
  provider_->AddMatchToMap(
      prerender_suggestion, std::string(), AutocompleteInput(),
      template_url.get(), client_->GetTemplateURLService()->search_terms_data(),
      TemplateURLRef::NO_SUGGESTION_CHOSEN,
      /*mark_as_deletable=*/false,
      /*in_keyword_mode=*/false, &map);

  ASSERT_EQ(1U, map.size());
  ASSERT_TRUE(provider_->matches().empty());

  AutocompleteMatch match = map.begin()->second;
  ASSERT_EQ(1U, match.duplicate_matches.size());
  EXPECT_TRUE(BaseSearchProvider::ShouldPrerender(match));
}
