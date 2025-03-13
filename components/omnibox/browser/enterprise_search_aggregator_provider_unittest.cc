// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
      : EnterpriseSearchAggregatorProvider(client, listener),
        update_results_future_(
            std::make_unique<base::test::TestFuture<void>>()) {}

  using EnterpriseSearchAggregatorProvider::CreateMatch;
  using EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider;
  using EnterpriseSearchAggregatorProvider::IsProviderAllowed;
  using EnterpriseSearchAggregatorProvider::
      ParseEnterpriseSearchAggregatorSearchResults;
  using EnterpriseSearchAggregatorProvider::RequestCompleted;

  using EnterpriseSearchAggregatorProvider::adjusted_input_;
  using EnterpriseSearchAggregatorProvider::done_;
  using EnterpriseSearchAggregatorProvider::matches_;
  using EnterpriseSearchAggregatorProvider::template_url_;

  void UpdateResults(const std::optional<base::Value::Dict>& response_value,
                     const int response_code) override {
    EnterpriseSearchAggregatorProvider::UpdateResults(std::move(response_value),
                                                      response_code);
    update_results_future_->SetValue();
  }

  bool WaitForUpdateResults() { return update_results_future_->Wait(); }

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
  std::unique_ptr<base::test::TestFuture<void>> update_results_future_;
};

const std::string kGoodJsonResponse = base::StringPrintf(
    R"({
        "querySuggestions": [
          {
            "suggestion": "John's Document 1",
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
                  "url": "https://example.com/image.png"
                }
              }
            },
            "dataStore": "project 1"
          }
        ],
        "contentSuggestions": [
          {
            "suggestion": "John's doodle",
            "contentType": "THIRD_PARTY",
            "document": {
              "name": "Document 2",
              "derivedStructData": {
                "source_type": "jira",
                "entity_type": "issue",
                "title": "John's doodle",
                "link": "https://www.example.com"
              }
            },
            "iconUri": "https://example.com/icon.png",
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
            "suggestion": "John's Document 1",
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
                "title": "John's doodle'",
                "link": "www.example.com"
              }
            }
          }
        ]
        })");

const std::string kNonDictJsonResponse =
    base::StringPrintf(R"(["test","result1","result2"])");

// Helper methods to dynamically generate valid responses.
std::string CreateQueryResult(const std::string& query) {
  return base::StringPrintf(
      R"(
        {"suggestion": "%s"}
        )",
      query);
}
std::string CreatePeopleResult(const std::string& displayName,
                               const std::string& userName,
                               const std::string& givenName,
                               const std::string& familyName) {
  return base::StringPrintf(
      R"(
        {
          "document": {
            "derivedStructData": {
              "name": {
                "displayName": "%s",
                "userName": "%s",
                "givenName": "%s",
                "familyName": "%s"
              }
            }
          }
        }
            )",
      displayName, userName, givenName, familyName);
}
std::string CreateContentResult(const std::string& title,
                                const std::string& mime_type,
                                const std::string& url) {
  return base::StringPrintf(
      R"(
        {
          "document": {
            "derivedStructData": {
              "title": "%s",
              "mime_type": "%s",
              "link": "%s"
            }
          }
        }
        )",
      title, mime_type, url);
}
std::string CreateResponse(std::vector<std::string> queries,
                           std::vector<std::string> peoples,
                           std::vector<std::string> contents) {
  auto jointQueries = base::JoinString(queries, ",");
  auto jointPeoples = base::JoinString(peoples, ",");
  auto jointContents = base::JoinString(contents, ",");
  return base::StringPrintf(
      R"(
          {
            "querySuggestions": [%s],
            "peopleSuggestions": [%s],
            "contentSuggestions": [%s]
          }
        )",
      jointQueries, jointPeoples, jointContents);
}

AutocompleteInput CreateInput(const std::u16string& text,
                              bool in_keyword_mode) {
  AutocompleteInput input = {text, metrics::OmniboxEventProto::OTHER,
                             TestSchemeClassifier()};
  if (in_keyword_mode) {
    input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  }
  return input;
}

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
    EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
        .WillRepeatedly(Return(true));
  }

  void InitFeature() { scoped_config_.Get().enabled = true; }

  void InitTemplateUrlService() {
    TemplateURLData data;
    data.SetShortName(u"keyword");
    data.SetKeyword(u"keyword");
    data.SetURL("https://www.google.com/?q={searchTerms}");
    data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    data.featured_by_policy = true;
    data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
    provider_->template_url_ = client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(data));
  }

  AutocompleteMatch CreateAutocompleteMatch(std::u16string fill_into_edit) {
    AutocompleteMatch match(provider_.get(), 1000, false,
                            AutocompleteMatchType::NAVSUGGEST);
    match.fill_into_edit = fill_into_edit;
    return match;
  }

  void ParseResponse(const std::string& response_string) {
    provider_->matches_.clear();
    std::optional<base::Value::Dict> response =
        base::JSONReader::ReadDict(response_string);
    ASSERT_TRUE(response);
    provider_->ParseEnterpriseSearchAggregatorSearchResults(*response);
  }

  std::vector<std::u16string> GetMatches() {
    std::vector<std::u16string> matches;
    for (const auto& m : provider_->matches_)
      matches.push_back(m.fill_into_edit);
    return matches;
  }

  using ScoredMatch = std::pair<std::u16string, int>;
  std::vector<ScoredMatch> GetScoredMatches() {
    std::vector<ScoredMatch> matches;
    for (const auto& m : provider_->matches_)
      matches.emplace_back(m.fill_into_edit, m.relevance);
    return matches;
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
  provider_->adjusted_input_ = CreateInput(u"input text", true);

  auto match = provider_->CreateMatch(
      AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY, true, 1000,
      "https://url.com", "https://example.com/image.png",
      "https://example.com/icon.png", u"title", u"additional text");
  EXPECT_EQ(match.relevance, 1000);
  EXPECT_EQ(match.destination_url.spec(), "https://url.com/");
  EXPECT_EQ(match.fill_into_edit, u"https://url.com");
  EXPECT_EQ(match.enterprise_search_aggregator_type,
            AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY);
  EXPECT_EQ(match.description, u"title");
  EXPECT_EQ(match.contents, u"additional text");
  EXPECT_EQ(match.image_url.spec(), "https://example.com/image.png");
  EXPECT_EQ(match.icon_url.spec(), "https://example.com/icon.png");
  EXPECT_EQ(match.keyword, u"keyword");
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(match.transition, ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(match.from_keyword, true);
}

// Test that the provider runs only when allowed.
TEST_F(EnterpriseSearchAggregatorProviderTest, IsProviderAllowed) {
  AutocompleteInput input = CreateInput(u"text text", false);

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
    // Improve Search Suggest setting should be enabled.
    EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
        .WillRepeatedly(Return(false));
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
        .WillRepeatedly(Return(true));
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }

  {
    // Feature must be enabled.
    scoped_config_.Get().enabled = false;
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    scoped_config_.Get().enabled = true;
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }

  {
    // The provider is only run if Google is the default search provider.
    TemplateURLService* turl_service = client_->GetTemplateURLService();
    TemplateURLData data;
    data.SetShortName(u"test");
    data.SetURL("https://www.yahoo.com/?q={searchTerms}");
    data.suggestions_url = "https://www.yahoo.com/complete/?q={searchTerms}";
    TemplateURL* new_default_provider =
        turl_service->Add(std::make_unique<TemplateURL>(data));
    turl_service->SetUserSelectedDefaultSearchProvider(new_default_provider);
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    TemplateURL* current_template_url =
        const_cast<TemplateURL*>((provider_->template_url_).get());
    turl_service->SetUserSelectedDefaultSearchProvider(current_template_url);
    turl_service->Remove(new_default_provider);
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }

  {
    // Query must be at least 4 characters long in unscoped mode.
    AutocompleteInput unscoped_input_long(
        u"text", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    EXPECT_TRUE(provider_->IsProviderAllowed(unscoped_input_long));
    AutocompleteInput unscoped_input_short(
        u"t", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsProviderAllowed(unscoped_input_short));
    AutocompleteInput unscoped_empty_input(
        u"", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsProviderAllowed(unscoped_empty_input));
  }

  {
    // Query must not be a url in unscoped mode
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
    AutocompleteInput query_input(u"https", metrics::OmniboxEventProto::OTHER,
                                  TestSchemeClassifier());
    EXPECT_TRUE(provider_->IsProviderAllowed(query_input));
    AutocompleteInput person_query(
        u"john doe", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    EXPECT_TRUE(provider_->IsProviderAllowed(person_query));
    AutocompleteInput url_input(u"www.web.site",
                                metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsProviderAllowed(url_input));
    AutocompleteInput url_no_prefix(
        u"john.com", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsProviderAllowed(url_no_prefix));
  }
}

// Test that a call to `Start()` will stop old requests to prevent their results
// from appearing with the new input.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStop) {
  scoped_config_.Get().enabled = false;
  AutocompleteInput invalid_input = CreateInput(u"keyword text", false);
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
  provider_->adjusted_input_ = CreateInput(u"keyword", true);
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->Start(provider_->adjusted_input_, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test that a call to `Start()` will not send a new request if input is zero
// suggest.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStopForZeroSuggest) {
  AutocompleteInput input = CreateInput(u"", false);
  input.set_focus_type(metrics::INTERACTION_FOCUS);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test that a call to `Start()` will not set `done_` if
// `omit_asynchronous_matches` is true.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       StartLeavesDoneForOmitAsynchronousMatches) {
  AutocompleteInput input = CreateInput(u"keyword text", false);
  input.set_omit_asynchronous_matches(true);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(0);

  provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

// Test response is parsed accurately.
TEST_F(EnterpriseSearchAggregatorProviderTest, Parse) {
  provider_->adjusted_input_ = CreateInput(u"john d", true);
  ParseResponse(kGoodJsonResponse);

  ACMatches matches = provider_->matches_;
  ASSERT_EQ(matches.size(), 3u);

  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[0].relevance, 600);
  EXPECT_EQ(matches[0].contents, u"john@example.com");
  EXPECT_EQ(matches[0].description, u"John Doe");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://www.google.com/?q=john%40example.com"));
  EXPECT_EQ(matches[0].image_url, GURL("https://example.com/image.png"));

  EXPECT_EQ(matches[1].type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(matches[1].relevance, 500);
  EXPECT_EQ(matches[1].contents, u"John's Document 1");
  EXPECT_EQ(matches[1].description, u"");
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://www.google.com/?q=John%27s+Document+1"));

  EXPECT_EQ(matches[2].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[2].relevance, 500);
  EXPECT_EQ(matches[2].contents, u"");
  EXPECT_EQ(matches[2].description, u"John's doodle");
  EXPECT_EQ(matches[2].destination_url, GURL("https://www.example.com"));
  EXPECT_EQ(matches[2].icon_url, GURL("https://example.com/icon.png"));
}

// Test results with missing expected fields are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithMissingFields) {
  provider_->adjusted_input_ = CreateInput(u"john d", true);
  ParseResponse(kMissingFieldsJsonResponse);
  EXPECT_THAT(
      GetMatches(),
      testing::ElementsAre(u"https://www.google.com/?q=john%40example.com",
                           u"https://www.google.com/?q=John%27s+Document+1",
                           u"www.example.com"));
}

// Test non-dict results are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithNonDict) {
  AutocompleteInput input = CreateInput(u"john d", true);
  provider_->adjusted_input_ = input;

  // Matches are not updated when response is not a json.
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(0);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(1);

  provider_->done_ = false;
  provider_->RequestCompleted(
      nullptr, 200, std::make_unique<std::string>(kNonDictJsonResponse));
  ASSERT_TRUE(provider_->WaitForUpdateResults());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_Start) {
  // Set a cached match.
  AutocompleteInput input = CreateInput(u"john d", false);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  // Call `Start()`, old match should still be present.
  provider_->Start(input, false);
  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://cached.org"));
}

// Test matches are cached and cleared in the appropriate flows. Expect the
// cached match to be cleared for scoped error responses.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_ErrorResponse) {
  // Set cached matches.
  AutocompleteInput input = CreateInput(u"john d", true);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with error, old match should be cleared.
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
  AutocompleteInput input = CreateInput(u"john d", false);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with error, old match should be cleared.
  provider_->done_ = false;
  provider_->RequestCompleted(nullptr, 404,
                              std::make_unique<std::string>("bad"));
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest, CacheMatches_EmptyResponse) {
  // Set a cached match.
  AutocompleteInput input = CreateInput(u"john d", false);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  // Matches are updated (cleared) when response is empty.
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with empty results, old match should be cleared.
  provider_->done_ = false;
  provider_->RequestCompleted(
      nullptr, 200, std::make_unique<std::string>(kGoodEmptyJsonResponse));
  ASSERT_TRUE(provider_->WaitForUpdateResults());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test matches are cached and cleared in the appropriate flows.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       CacheMatches_SuccessfulResponse) {
  // Set a cached match.
  AutocompleteInput input = CreateInput(u"john d", false);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org")};

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  // Complete request with non-empty results, old match should be replaced.
  provider_->done_ = false;
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  ASSERT_TRUE(provider_->WaitForUpdateResults());
  EXPECT_THAT(
      GetMatches(),
      testing::ElementsAre(u"https://www.google.com/?q=john%40example.com",
                           u"https://www.google.com/?q=John%27s+Document+1",
                           u"https://www.example.com"));
}

// Test things work when using an unfeatured keyword.
TEST_F(EnterpriseSearchAggregatorProviderTest, UnfeaturedKeyword) {
  // Unfeatured keyword must match a featured keyword according to current
  // design.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"unfeatured");
  turl_data.SetKeyword(u"unfeatured");
  turl_data.SetURL("http://www.yahoo.com/{searchTerms}");
  turl_data.is_active = TemplateURLData::ActiveStatus::kTrue;
  turl_data.featured_by_policy = false;
  turl_data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
  client_->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(turl_data));
  AutocompleteInput input = CreateInput(u"unfeatured john d", true);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  ASSERT_TRUE(provider_->WaitForUpdateResults());
  EXPECT_EQ(provider_->matches_[0].keyword, u"unfeatured");
  EXPECT_THAT(GetMatches(), testing::ElementsAre(
                                u"http://www.yahoo.com/john@example.com",
                                u"http://www.yahoo.com/John's%20Document%201",
                                u"https://www.example.com"));
}

// Test things work in unscoped mode.
TEST_F(EnterpriseSearchAggregatorProviderTest, UnscopedMode) {
  AutocompleteInput input = CreateInput(u"john d", false);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(true, provider_.get()))
      .Times(1);
  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(false, provider_.get()))
      .Times(0);

  provider_->Start(input, false);
  provider_->RequestCompleted(nullptr, 200,
                              std::make_unique<std::string>(kGoodJsonResponse));
  ASSERT_TRUE(provider_->WaitForUpdateResults());
  EXPECT_THAT(
      GetMatches(),
      testing::ElementsAre(u"https://www.google.com/?q=john%40example.com",
                           u"https://www.google.com/?q=John%27s+Document+1",
                           u"https://www.example.com"));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, Limits) {
  // At most 2 per type when unscoped. Filtered matches shouldn't count against
  // the limit.
  provider_->adjusted_input_ = CreateInput(u"mango m", false);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("grape-1-query"),
          CreateQueryResult("grape-2-query"),
          CreateQueryResult("grape-3-query"),
          CreateQueryResult("mango-1-query"),
          CreateQueryResult("mango-2-query"),
          CreateQueryResult("mango-3-query"),
      },
      {
          CreatePeopleResult("displayName", "grape-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-3-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-3-people", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("grape-1-content", "mime_type", "url-grape-1"),
          CreateContentResult("grape-2-content", "mime_type", "url-grape-2"),
          CreateContentResult("grape-3-content", "mime_type", "url-grape-3"),
          CreateContentResult("mango-1-content", "mime_type", "url-mango-1"),
          CreateContentResult("mango-2-content", "mime_type", "url-mango-2"),
          CreateContentResult("mango-3-content", "mime_type", "url-mango-3"),
      }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://www.google.com/?q=mango-1-people", 600},
          ScoredMatch{u"https://www.google.com/?q=mango-2-people", 600},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 500},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 500},
          ScoredMatch{u"url-mango-1", 500}, ScoredMatch{u"url-mango-2", 500}));

  // Types that have less than 2 results aren't backfilled by other types.
  provider_->adjusted_input_ = CreateInput(u"mango m", false);
  ParseResponse(CreateResponse(
      {}, {},
      {
          CreateContentResult("grape-1-content", "mime_type", "url-grape-1"),
          CreateContentResult("grape-2-content", "mime_type", "url-grape-2"),
          CreateContentResult("grape-3-content", "mime_type", "url-grape-3"),
          CreateContentResult("mango-1-content", "mime_type", "url-mango-1"),
          CreateContentResult("mango-2-content", "mime_type", "url-mango-2"),
          CreateContentResult("mango-3-content", "mime_type", "url-mango-3"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url-mango-1", 500},
                                   ScoredMatch{u"url-mango-2", 500}));

  // The best 2 suggestions should be shown, even if they're not
  // the 1st 2.
  provider_->adjusted_input_ = CreateInput(u"mango mango-2 mango-3", false);
  ParseResponse(CreateResponse(
      {}, {},
      {
          CreateContentResult("mango-1-content", "mime_type", "url-mango-1"),
          CreateContentResult("mango-2-content", "mime_type", "url-mango-2"),
          CreateContentResult("mango-3-content", "mime_type", "url-mango-3"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url-mango-2", 500},
                                   ScoredMatch{u"url-mango-3", 500}));

  // Can show more than 2 per type when scoped.
  provider_->adjusted_input_ = CreateInput(u"mango m", true);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("grape-1-query"),
          CreateQueryResult("grape-2-query"),
          CreateQueryResult("grape-3-query"),
          CreateQueryResult("mango-1-query"),
          CreateQueryResult("mango-2-query"),
          CreateQueryResult("mango-3-query"),
      },
      {
          CreatePeopleResult("displayName", "grape-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-3-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-3-people", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("grape-1-content", "mime_type", "url-grape-1"),
          CreateContentResult("grape-2-content", "mime_type", "url-grape-2"),
          CreateContentResult("grape-3-content", "mime_type", "url-grape-3"),
          CreateContentResult("mango-1-content", "mime_type", "url-mango-1"),
          CreateContentResult("mango-2-content", "mime_type", "url-mango-2"),
          CreateContentResult("mango-3-content", "mime_type", "url-mango-3"),
      }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://www.google.com/?q=mango-1-people", 600},
          ScoredMatch{u"https://www.google.com/?q=mango-2-people", 600},
          ScoredMatch{u"https://www.google.com/?q=mango-3-people", 600},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 500},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 500},
          ScoredMatch{u"https://www.google.com/?q=mango-3-query", 500},
          ScoredMatch{u"url-mango-1", 500}, ScoredMatch{u"url-mango-2", 500},
          ScoredMatch{u"url-mango-3", 500}));

  // Limit low-quality suggestions. Only the 1st 2 matches are allowed to score
  // lower than 500. Even if the 1st 2 matches score higher than 500, the
  // remaining matches must also score higher than 500.
  provider_->adjusted_input_ = CreateInput(u"m ma", true);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("grape-1-query"),
          CreateQueryResult("grape-2-query"),
          CreateQueryResult("grape-3-query"),
          CreateQueryResult("mango-1-query"),
          CreateQueryResult("mango-2-query"),
          CreateQueryResult("mango-3-query"),
      },
      {
          CreatePeopleResult("displayName", "grape-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "grape-3-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-1-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-2-people", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "mango-3-people", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("grape-1-content", "mime_type", "url-grape-1"),
          CreateContentResult("grape-2-content", "mime_type", "url-grape-2"),
          CreateContentResult("grape-3-content", "mime_type", "url-grape-3"),
          CreateContentResult("mango-1-content", "mime_type", "url-mango-1"),
          CreateContentResult("mango-2-content", "mime_type", "url-mango-2"),
          CreateContentResult("mango-3-content", "mime_type", "url-mango-3"),
      }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://www.google.com/?q=mango-1-people", 300},
          ScoredMatch{u"https://www.google.com/?q=mango-2-people", 300}));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, Relevance) {
  // Results that don't match the input should be filtered out.
  provider_->adjusted_input_ = CreateInput(u"match m", true);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("query"),
          CreateQueryResult("matchQuery"),
      },
      {
          CreatePeopleResult("displayName", "userName", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "matchUserName", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("title", "mime_type", "url"),
          CreateContentResult("matchTitle", "mime_type", "url"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://www.google.com/?q=matchUserName", 600},
                  ScoredMatch{u"https://www.google.com/?q=matchQuery", 500},
                  ScoredMatch{u"url", 500}));

  // Score using weighted sum of matches.
  provider_->adjusted_input_ = CreateInput(u"zero on tw th", true);
  ParseResponse(CreateResponse(
      {}, {},
      {
          CreateContentResult("zero", "mime_type", "url-0"),
          CreateContentResult("zero one", "mime_type", "url-01"),
          CreateContentResult("zero one two", "mime_type", "url-012"),
          CreateContentResult("zero one two three", "mime_type", "url-0123"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url-0123", 700},
                                   ScoredMatch{u"url-012", 600},
                                   ScoredMatch{u"url-01", 500}));

  // Duplicate matches do not count.
  // - If the input repeats a word, only 1 should count.
  // - If the result field repeats a word, only 1 should count.
  // - If a word appears in multiple result fields, only 1 should count.
  provider_->adjusted_input_ = CreateInput(u"one one", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one one", "one one", "url-1"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url-1", 400}));

  // Each input word can match only 1 result word.
  provider_->adjusted_input_ = CreateInput(u"one one", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one oneTwo", "mime_type", "url"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 400}));

  // A result word can match multiple input words. This is just a side effect
  // of the implementation rather than intentional design.
  provider_->adjusted_input_ = CreateInput(u"one on o", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one", "mime_type", "url"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 600}));

  // Matches outside contents and description contribute less to the score.
  provider_->adjusted_input_ = CreateInput(u"one two three four five", true);
  ParseResponse(CreateResponse(
      {}, {},
      {
          CreateContentResult("title one", "two three four five", "inside"),
          CreateContentResult("title", "one two three four five", "outside"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"inside", 800},
                                   ScoredMatch{u"outside", 500}));

  // Short input words contribute less to the score.
  provider_->adjusted_input_ = CreateInput(u"on two three four five", true);
  ParseResponse(CreateResponse(
      {}, {},
      {
          CreateContentResult("one", "two three four five", "url"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 500}));

  // Matches outside human-readable fields aren't considered in scoring.
  provider_->adjusted_input_ = CreateInput(u"title url", true);
  ParseResponse(CreateResponse({}, {},
                               {
                                   CreateContentResult("title", "mime", "url1"),
                                   CreateContentResult("title", "mime", "url2"),
                               }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url1", 400},
                                   ScoredMatch{u"url2", 400}));

  // Suggestions that match every input words, when there are at least 2, should
  // be scored higher.
  provider_->adjusted_input_ = CreateInput(u"one two", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "mime", "url"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 1000}));

  // Suggestions that match every input words, when there is not at least 2,
  // should not be scored higher.
  provider_->adjusted_input_ = CreateInput(u"one", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "mime", "url"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 400}));

  // Suggestions that match at least 2 but not all inputs words should not be
  // scored higher.
  provider_->adjusted_input_ = CreateInput(u"one two four", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "mime", "url"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 800}));

  // Require at least 1 strong match or 2 weak matches.
  provider_->adjusted_input_ = CreateInput(u"title", true);
  ParseResponse(CreateResponse({}, {},
                               {
                                   CreateContentResult("title", "mime", "url"),
                               }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url", 400}));

  // Require at least 1 strong match or 2 weak matches.
  provider_->adjusted_input_ = CreateInput(u"mimeA mimeB", true);
  ParseResponse(
      CreateResponse({}, {},
                     {
                         CreateContentResult("title", "mimeA", "url-1"),
                         CreateContentResult("title", "mimeA mimeB", "url-2"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"url-2", 200}));

  // Require at least half the input words to match.
  provider_->adjusted_input_ = CreateInput(u"title x y", true);
  ParseResponse(CreateResponse({}, {},
                               {
                                   CreateContentResult("title", "mime", "url"),
                               }));
  EXPECT_THAT(GetScoredMatches(), testing::ElementsAre());

  // People matches should be boosted.
  provider_->adjusted_input_ = CreateInput(u"query q", true);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("query"),
      },
      {
          CreatePeopleResult("displayName query", "userName", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "matchUserName", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("title query", "mime_type", "url"),
      }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://www.google.com/?q=userName", 600},
                  ScoredMatch{u"https://www.google.com/?q=query", 500},
                  ScoredMatch{u"url", 500}));

  // People matches must match all input words.
  provider_->adjusted_input_ = CreateInput(u"query q unmatched", true);
  ParseResponse(CreateResponse(
      {
          CreateQueryResult("query"),
      },
      {
          CreatePeopleResult("displayName query", "userName", "givenName",
                             "familyName"),
          CreatePeopleResult("displayName", "matchUserName", "givenName",
                             "familyName"),
      },
      {
          CreateContentResult("title query", "mime_type", "url"),
      }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(ScoredMatch{u"https://www.google.com/?q=query", 500},
                           ScoredMatch{u"url", 500}));
}
