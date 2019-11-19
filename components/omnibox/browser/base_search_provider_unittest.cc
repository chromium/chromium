// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
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
  MOCK_METHOD1(DeleteMatch, void(const AutocompleteMatch& match));
  MOCK_CONST_METHOD1(AddProviderInfo, void(ProvidersInfo* provider_info));
  MOCK_CONST_METHOD1(GetTemplateURL, const TemplateURL*(bool is_keyword));
  MOCK_CONST_METHOD1(GetInput, const AutocompleteInput(bool is_keyword));
  MOCK_CONST_METHOD1(ShouldAppendExtraParams,
                     bool(const SearchSuggestionParser::SuggestResult& result));
  MOCK_METHOD1(RecordDeletionResult, void(bool success));

  MOCK_METHOD2(Start,
               void(const AutocompleteInput& input, bool minimal_changes));
  void AddMatchToMap(const SearchSuggestionParser::SuggestResult& result,
                     const std::string& metadata,
                     int accepted_suggestion,
                     bool mark_as_deletable,
                     bool in_keyword_mode,
                     MatchMap* map) {
    BaseSearchProvider::AddMatchToMap(result,
                                      metadata,
                                      accepted_suggestion,
                                      mark_as_deletable,
                                      in_keyword_mode,
                                      map);
  }

 protected:
  ~TestBaseSearchProvider() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBaseSearchProvider);
};

class BaseSearchProviderTest : public testing::Test {
 public:
  ~BaseSearchProviderTest() override {}

 protected:
  void SetUp() override {
    std::unique_ptr<TemplateURLService> template_url_service(
        new TemplateURLService(
            nullptr, std::unique_ptr<SearchTermsData>(new SearchTermsData),
            nullptr, std::unique_ptr<TemplateURLServiceClient>(), nullptr,
            base::Closure()));
    client_.reset(new MockAutocompleteProviderClient());
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
  base::string16 query = base::ASCIIToUTF16("weather los angeles");
  SuggestionAnswer answer;
  answer.set_type(2334);

  EXPECT_CALL(*provider_, GetInput(_))
      .WillRepeatedly(Return(AutocompleteInput()));
  EXPECT_CALL(*provider_, GetTemplateURL(_))
      .WillRepeatedly(Return(template_url.get()));

  SearchSuggestionParser::SuggestResult more_relevant(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*subtype_identifier=*/0, /*from_keyword_provider=*/false,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  provider_->AddMatchToMap(
      more_relevant, std::string(), TemplateURLRef::NO_SUGGESTION_CHOSEN,
      false, false, &map);

  SearchSuggestionParser::SuggestResult less_relevant(
      query, AutocompleteMatchType::SEARCH_SUGGEST,
      /*subtype_identifier=*/0, /*from_keyword_provider=*/false,
      /*relevance=*/850, /*relevance_from_server=*/true,
      /*input_text=*/query);
  less_relevant.SetAnswer(answer);
  provider_->AddMatchToMap(
      less_relevant, std::string(), TemplateURLRef::NO_SUGGESTION_CHOSEN,
      false, false, &map);

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
      /*subtype_identifier=*/0, /*from_keyword_provider=*/false,
      /*relevance=*/1300, /*relevance_from_server=*/true,
      /*input_text=*/query);
  more_relevant.SetAnswer(answer2);
  provider_->AddMatchToMap(
      more_relevant, std::string(), TemplateURLRef::NO_SUGGESTION_CHOSEN,
      false, false, &map);
  provider_->AddMatchToMap(
      less_relevant, std::string(), TemplateURLRef::NO_SUGGESTION_CHOSEN,
      false, false, &map);
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
      base::ASCIIToUTF16("weather"), 7, metrics::OmniboxEventProto::BLANK,
      TestSchemeClassifier());

  EXPECT_CALL(*provider_, GetInput(_))
      .WillRepeatedly(Return(autocomplete_input));
  EXPECT_CALL(*provider_, GetTemplateURL(_))
      .WillRepeatedly(Return(template_url.get()));

  base::string16 query = base::ASCIIToUTF16("angeles now");
  base::string16 suggestion = base::ASCIIToUTF16("weather los ") + query;
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
      /*subtype_identifier=*/0,
      /*match_contents=*/query,
      /*match_contents_prefix=*/base::ASCIIToUTF16("..."),
      /*annotation=*/base::string16(),
      /*suggest_query_params=*/std::string(),
      /*deletion_url=*/std::string(),
      /*image_dominant_color=*/std::string(),
      /*image_url=*/std::string(),
      /*from_keyword_provider=*/false,
      /*relevance=*/1300,
      /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*input_text=*/query);

  TestBaseSearchProvider::MatchMap map;
  provider_->AddMatchToMap(suggest_result, std::string(),
                           TemplateURLRef::NO_SUGGESTION_CHOSEN, false, false,
                           &map);

  ASSERT_EQ(1UL, map.size());
  const auto& entry = *(map.begin());
  std::string text =
      entry.second.GetAdditionalInfo(kACMatchPropertyContentsStartIndex);
  size_t length;
  EXPECT_TRUE(base::StringToSizeT(text, &length));
  text = entry.second.GetAdditionalInfo(kACMatchPropertySuggestionText);
  EXPECT_GE(text.length(), length);
}
