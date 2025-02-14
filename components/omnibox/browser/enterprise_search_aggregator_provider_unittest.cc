// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
using testing::_;
using testing::Return;
}  // namespace

class FakeEnterpriseSearchAggregatorProvider
    : public EnterpriseSearchAggregatorProvider {
 public:
  explicit FakeEnterpriseSearchAggregatorProvider(
      AutocompleteProviderClient* client,
      AutocompleteProviderListener* listener)
      : EnterpriseSearchAggregatorProvider(client, listener) {}

  using EnterpriseSearchAggregatorProvider::SuggestionType;

  using EnterpriseSearchAggregatorProvider::CreateMatch;
  using EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider;
  using EnterpriseSearchAggregatorProvider::IsProviderAllowed;
  using EnterpriseSearchAggregatorProvider::
      ParseEnterpriseSearchAggregatorSearchResults;
  using EnterpriseSearchAggregatorProvider::RequestCompleted;
  using EnterpriseSearchAggregatorProvider::UpdateResults;

  using EnterpriseSearchAggregatorProvider::done_;
  using EnterpriseSearchAggregatorProvider::input_;
  using EnterpriseSearchAggregatorProvider::matches_;

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
};

const std::string kGoodJsonResponse = base::StringPrintf(
    R"({
        "querySuggestions": [
          {
            "suggestion": "Document 1",
            "dataStore": []
          }
        ],
        "peopleSuggestions": [
          {
            "document": {
              "name": "sundar",
              "derivedStructData": {
                "name": {
                  "display_name_lower": "john doe",
                  "familyName": "Doe",
                  "givenName": "John",
                  "given_name_lower": "john",
                  "family_name_lower": "doe",
                  "displayName": "John Doe",
                  "userName": "john@example.com"
                },
                "emails": [
                  {
                    "type": "primary",
                    "value": "john@example.com"
                  }
                ],
                "displayPhoto": {
                  "url": "www.example.com"
                }
              }
            },
            "dataStore": "project 1"
          }
        ],
        "contentSuggestions": [
          {
            "suggestion": "critical crash",
            "contentType": "THIRD_PARTY",
            "document": {
              "name": "Document 2",
              "structData": {
                "title": "Critical Crash",
                "uri": "www.example.com"
              },
              "derivedStructData": {
                "source_type": "jira",
                "entity_type": "issue",
                "title": "Critical Crash",
                "link": "https://www.example.com"
              }
            },
            "dataStore": "project2"
          }
        ]
      })");

const std::string kGoodEmptyJsonResponse = base::StringPrintf(
    R"({
        "querySuggestions": [],
        "peopleSuggestions": [],
        "contentSuggestions": []
      })");

const std::string kMissingFieldsJsonResponse = base::StringPrintf(
    R"({
        "querySuggestions": [
          {
            "suggestion": "",
            "dataStore": []
          },
          {
            "suggestion": "Document 1",
            "dataStore": []
          }
        ],
        "peopleSuggestions": [
          {
            "document": {
              "derivedStructData": {
                "name": {
                  "userName": "missingDisplayName@example.com"
                }
              }
            }
          },
          {
            "document": {
              "derivedStructData": {
                "name": {
                  "displayName": "Missing user name"
                }
              }
            }
          },
          {
            "document": {
              "derivedStructData": {
                "name": {
                  "displayName": "John Doe",
                  "userName": "john@example.com"
                }
              }
            }
          }
        ],
        "contentSuggestions": [
          {
            "document": {
              "derivedStructData": {
                "title": "Missing URI"
              }
            }
          },
          {
            "document": {
              "name": "Document 2",
              "derivedStructData": {
                "link": "www.missingTitle.com"
              }
            }
          },
          {
            "document": {
              "name": "Document 2",
              "derivedStructData": {
                "title": "Critical Crash",
                "link": "www.example.com"
              }
            }
          }
        ]
        })");

const std::string kNonDictJsonResponse =
    base::StringPrintf(R"(["test","result1","result2"])");

class MockAutocompleteProviderListener : public AutocompleteProviderListener {
 public:
  MOCK_METHOD(void,
              OnProviderUpdate,
              (bool updated_matches, const AutocompleteProvider* provider),
              (override));
};
class EnterpriseSearchAggregatorProviderTest : public testing::Test {
 protected:
  EnterpriseSearchAggregatorProviderTest() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    mock_listener_ = std::make_unique<MockAutocompleteProviderListener>();
    provider_ = new FakeEnterpriseSearchAggregatorProvider(
        client_.get(), mock_listener_.get());
    InitClient();
    InitFeature();
    InitTemplateUrlService();
  }

  void InitClient() {
    EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  }

  void InitFeature() {
    scoped_config_.Get().enabled = true;
    scoped_config_.Get().name = "keyword";
    scoped_config_.Get().shortcut = "keyword";
    scoped_config_.Get().search_url = "example.com/{searchTerms}";
    scoped_config_.Get().suggest_url = "example.com";
  }

  void InitTemplateUrlService() {
    TemplateURLData data;
    data.SetShortName(u"keyword");
    data.SetKeyword(u"keyword");
    data.SetURL("http://www.yahoo.com/{searchTerms}");
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    data.featured_by_policy = true;
    data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
    client_->GetTemplateURLService()->Add(std::make_unique<TemplateURL>(data));
  }

  std::vector<std::u16string> GetMatches() {
    std::vector<std::u16string> match_strings;
    for (const auto& m : provider_->matches_)
      match_strings.push_back(m.fill_into_edit);
    return match_strings;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  std::unique_ptr<MockAutocompleteProviderListener> mock_listener_;
  scoped_refptr<FakeEnterpriseSearchAggregatorProvider> provider_;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;
};

TEST_F(EnterpriseSearchAggregatorProviderTest, CreateMatch) {
  AutocompleteInput input{u"input text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier()};
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);

  auto match = provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1000,
      "https://url.com", u"title", u"additional text");
  EXPECT_EQ(match.destination_url.spec(), "https://url.com/");
  EXPECT_EQ(match.fill_into_edit, u"https://url.com");
  EXPECT_EQ(match.description, u"title");
  EXPECT_EQ(match.contents, u"additional text");
  EXPECT_EQ(match.keyword, u"keyword");
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(match.transition, ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(match.from_keyword, true);
}

// Test that the provider runs only when allowed.
TEST_F(EnterpriseSearchAggregatorProviderTest, IsProviderAllowed) {
  AutocompleteInput input(u"text text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // Check `IsProviderAllowed()` returns true when all conditions pass.
  EXPECT_TRUE(provider_->IsProviderAllowed(input));

  {
    // Should not be an incognito window.
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }

  {
    // Feature must be enabled.
    scoped_config_.Get().enabled = false;
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    scoped_config_.Get().enabled = true;
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }
}

// Test that a call to `Start()` will stop old requests to prevent their results
// from appearing with the new input.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStop) {
  scoped_config_.Get().enabled = false;
  AutocompleteInput invalid_input(u"keyword text",
                                  metrics::OmniboxEventProto::OTHER,
                                  TestSchemeClassifier());
  invalid_input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->done_ = false;
  provider_->Start(invalid_input, false);
  EXPECT_TRUE(provider_->done());
}

// Test that a call to `Start()` will clear cached matches and not send a new
// request if input (scoped) is empty.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       StartCallsStopForScopedEmptyInput) {
  AutocompleteInput input(u"keyword ", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test that a call to `Start()` will not send a new  request if input is zero
// suggest.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStopForZeroSuggest) {
  AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::INTERACTION_FOCUS);
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  // Matches will not be cleared but the provider will not be called for Zero
  // Suggest.
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://cached.org"));
}

// Test that a call to `Start()` will not set `done_` if
// `omit_asynchronous_matches` is true.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       StartLeavesDoneForOmitAsynchronousMatches) {
  AutocompleteInput input(u"keyword text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

// Test response is parsed accurately.
TEST_F(EnterpriseSearchAggregatorProviderTest, Parse) {
  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJsonResponse);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());
  AutocompleteInput input{u"keyword text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier()};
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);

  provider_->input_ = input;

  provider_->ParseEnterpriseSearchAggregatorSearchResults(*response);
  ACMatches matches = provider_->matches_;

  ASSERT_EQ(matches.size(), 3u);

  for (int i = 0; i < int(matches.size()); i++) {
    EXPECT_EQ(matches[i].relevance, 1000 - i);
  }

  EXPECT_EQ(matches[0].type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(matches[0].contents, u"Document 1");
  EXPECT_EQ(matches[0].description, u"");
  EXPECT_EQ(matches[0].destination_url,
            GURL("http://www.yahoo.com/Document%201"));

  EXPECT_EQ(matches[1].type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(matches[1].contents, u"John Doe");
  EXPECT_EQ(matches[1].description, u"john@example.com");
  EXPECT_EQ(matches[1].destination_url,
            GURL("http://www.yahoo.com/john@example.com"));

  EXPECT_EQ(matches[2].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[2].contents, u"");
  EXPECT_EQ(matches[2].description, u"Critical Crash");
  EXPECT_EQ(matches[2].destination_url, GURL("https://www.example.com"));
}

// Test results with missing expected fields are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithMissingFields) {
  std::optional<base::Value> response =
      base::JSONReader::Read(kMissingFieldsJsonResponse);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());
  AutocompleteInput input{u"keyword text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier()};
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);

  provider_->input_ = input;
  provider_->ParseEnterpriseSearchAggregatorSearchResults(*response);

  EXPECT_THAT(GetMatches(),
              testing::ElementsAre(u"http://www.yahoo.com/Document%201",
                                   u"http://www.yahoo.com/john@example.com",
                                   u"www.example.com"));
}

// Test non-dict results are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithNonDict) {
  AutocompleteInput input{u"keyword text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier()};
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);

  provider_->input_ = input;
  provider_->UpdateResults(kNonDictJsonResponse);

  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_Start) {
  // Set a cached match.
  AutocompleteInput input(u"keyword query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  // Call `Start()`, old match should still be present.
  provider_->Start(input, false);
  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://cached.org"));
}

// Test matches are cached and cleared in the appropriate flows. Expect the
// cached match to be cleared for scoped error responses.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_ErrorResponse) {
  // Set cached matches.
  AutocompleteInput input(u"keyword q", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with error, old match should be cleared.
  provider_->input_ = input;
  provider_->done_ = false;
  provider_->RequestCompleted(nullptr, 404,
                              std::make_unique<std::string>("bad"));
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows. Expect the
// cached match to be cleared for unscoped error responses
TEST_F(EnterpriseSearchAggregatorProviderTest,
       CacheMatches_ErrorResponse_Unscoped) {
  // Set cached matches.
  AutocompleteInput input(u"keyword q", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with error, old match should be cleared.
  provider_->input_ = input;
  provider_->done_ = false;
  provider_->RequestCompleted(nullptr, 404,
                              std::make_unique<std::string>("bad"));
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_EmptyResponse) {
  // Set a cached match.
  AutocompleteInput input(u"keyword query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(0);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(1);

  // Complete request with empty results, old match should be cleared.
  provider_->input_ = input;
  provider_->done_ = false;
  provider_->RequestCompleted(
      nullptr, 200, std::make_unique<std::string>(kGoodEmptyJsonResponse));
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       CacheMatches_SuccessfulResponse) {
  // Set a cached match.
  AutocompleteInput input(u"keyword query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->matches_ = {provider_->CreateMatch(
      input, u"keyword",
      FakeEnterpriseSearchAggregatorProvider::SuggestionType::QUERY, true, 1500,
      "https://cached.org", u"cached", u"cached")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with non-empty results, old match should be replaced.
  provider_->input_ = input;
  provider_->done_ = false;
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  EXPECT_THAT(GetMatches(),
              testing::ElementsAre(u"http://www.yahoo.com/Document%201",
                                   u"http://www.yahoo.com/john@example.com",
                                   u"https://www.example.com"));
}

// Test things work when using an unfeatured keyword.
TEST_F(EnterpriseSearchAggregatorProviderTest, UnfeaturedKeyword) {
  // Unfeatured keyword must match a featured keyword according to current
  // design.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("http://www.yahoo.com/{searchTerms}");
  turl_data.is_active = TemplateURLData::ActiveStatus::kTrue;
  turl_data.featured_by_policy = false;
  turl_data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
  client_->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(turl_data));
  AutocompleteInput input(u"yahoo query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  EXPECT_THAT(GetMatches(),
              testing::ElementsAre(u"http://www.yahoo.com/Document%201",
                                   u"http://www.yahoo.com/john@example.com",
                                   u"https://www.example.com"));
}

// Test things work in unscoped mode.
TEST_F(EnterpriseSearchAggregatorProviderTest, UnscopedMode) {
  AutocompleteInput input(u"query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  EXPECT_THAT(GetMatches(),
              testing::ElementsAre(u"http://www.yahoo.com/Document%201",
                                   u"http://www.yahoo.com/john@example.com",
                                   u"https://www.example.com"));
}
