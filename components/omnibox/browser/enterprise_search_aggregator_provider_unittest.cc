// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/enterprise_search_aggregator_provider.h"

#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_enums.h"
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
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
using testing::_;
using testing::FieldsAre;
using testing::Return;
using SuggestionType = AutocompleteMatch::EnterpriseSearchAggregatorType;

// Concats optional strings to generate sliced and unsliced histogram names.
// E.g. X, X.Y1, X.Y2.Z.
std::string HistogramName(const std::string& base,
                          std::optional<std::string> type,
                          std::optional<bool> completed) {
  std::string name = base;
  if (type.has_value())
    name += "." + type.value();
  if (completed.has_value())
    name += completed.value() ? ".Completed" : ".Interrupted";
  return name;
}

// Helper to cleanly verify logging. Intentionally verifies all histograms; i.e.
// doesn't allow verifying 1 histogram is correct and not caring whether another
// histogram is logged. The default constructor `{}` expects no histogram to be
// logged. Can use either the constructor `{.unsliced_completed = true}` or
// direct assignment `expected_histogram.unsliced_completed = true` to set
// expectations.
//
// E.g.:
//  ExpectedHistogram expected_histogram{.query_completed = true};
//  // Complete query request.
//  expected_histogram.Verify();
//
//  // Interrupt provider.
//  expected_histogram.unsliced_interrupted = true;
//  expected_histogram.people_interrupted = true;
//  // ~ExpectedHistogram() will verify expectations.

struct ExpectedHistogram {
  ~ExpectedHistogram() { Verify(); }

  void Verify() {
    std::string kTimeHistogramName =
        "Omnibox.SuggestRequestsSent.ResponseTime2.RequestState."
        "EnterpriseSearchAggregatorSuggest";
    std::string kCountHistogramName =
        "Omnibox.SuggestRequestsSent.ResultCount."
        "EnterpriseSearchAggregatorSuggest";

    // Verify timing histograms.
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, {}, {}),
        unsliced_completed + unsliced_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, {}, true), unsliced_completed);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, {}, false), unsliced_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Query", {}),
        query_completed + query_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Query", true), query_completed);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Query", false), query_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "People", {}),
        people_completed + people_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "People", true), people_completed);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "People", false), people_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Content", {}),
        content_completed + content_interrupted);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Content", true), content_completed);
    histogram_tester.ExpectTotalCount(
        HistogramName(kTimeHistogramName, "Content", false),
        content_interrupted);

    // Verify count histograms.
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            HistogramName(kCountHistogramName, {}, {})),
        testing::ElementsAreArray(unsliced_count.has_value()
                                      ? std::vector<base::Bucket>{base::Bucket(
                                            unsliced_count.value(), 1)}
                                      : std::vector<base::Bucket>{}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            HistogramName(kCountHistogramName, "Query", {})),
        testing::ElementsAreArray(query_count.has_value()
                                      ? std::vector<base::Bucket>{base::Bucket(
                                            query_count.value(), 1)}
                                      : std::vector<base::Bucket>{}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            HistogramName(kCountHistogramName, "People", {})),
        testing::ElementsAreArray(people_count.has_value()
                                      ? std::vector<base::Bucket>{base::Bucket(
                                            people_count.value(), 1)}
                                      : std::vector<base::Bucket>{}));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            HistogramName(kCountHistogramName, "Content", {})),
        testing::ElementsAreArray(content_count.has_value()
                                      ? std::vector<base::Bucket>{base::Bucket(
                                            content_count.value(), 1)}
                                      : std::vector<base::Bucket>{}));
  }

  base::HistogramTester histogram_tester;

  // Timing histograms. If false, expected to not be logged. If true, expected
  // to log exactly once.
  bool unsliced_completed = false;
  bool unsliced_interrupted = false;
  bool query_completed = false;
  bool query_interrupted = false;
  bool people_completed = false;
  bool people_interrupted = false;
  bool content_completed = false;
  bool content_interrupted = false;

  // Count histograms. If `nullopt`, expected to not be logged. If not null,
  // even if 0, expected to log its value exactly once.
  std::optional<int> unsliced_count;
  std::optional<int> query_count;
  std::optional<int> people_count;
  std::optional<int> content_count;
};
}  // namespace

class FakeEnterpriseSearchAggregatorProvider
    : public EnterpriseSearchAggregatorProvider {
 public:
  FakeEnterpriseSearchAggregatorProvider(AutocompleteProviderClient* client,
                                         AutocompleteProviderListener* listener)
      : EnterpriseSearchAggregatorProvider(client, listener) {}
  using EnterpriseSearchAggregatorProvider::AggregateMatches;
  using EnterpriseSearchAggregatorProvider::CreateMatch;
  using EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider;
  using EnterpriseSearchAggregatorProvider::IsProviderAllowed;
  using EnterpriseSearchAggregatorProvider::RequestCompleted;
  using EnterpriseSearchAggregatorProvider::RequestStarted;

  using EnterpriseSearchAggregatorProvider::adjusted_input_;
  using EnterpriseSearchAggregatorProvider::done_;
  using EnterpriseSearchAggregatorProvider::matches_;
  using EnterpriseSearchAggregatorProvider::requests_;
  using EnterpriseSearchAggregatorProvider::template_url_;

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
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
            "suggestion": "john@example.com",
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
                  "userName": "john"
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
            "destinationUri": "https://example.com/people/jdoe",
            "score": 0.8,
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
                "link": "https://www.example.co.uk",
                "owner": "John Doe",
                "mime_type": "application/vnd.google-apps.document",
                "updated_time": 1192487100
              }
            },
            "destinationUri": "https://www.example.com",
            "iconUri": "https://example.com/icon.png",
            "score": 0.4,
            "dataStore": "project2"
          }
        ]
      })");

const std::string kQueryGoodJsonResponse = R"({
    "querySuggestions": [
      {
        "suggestion": "John's Document 1",
        "dataStore": []
      }
    ]
  })";

const std::string kPeopleGoodJsonResponse = R"({
    "peopleSuggestions": [
      {
        "suggestion": "john@example.com",
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
              "userName": "john"
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
        "destinationUri": "https://example.com/people/jdoe",
        "score": 0.8,
        "dataStore": "project 1"
      }
    ]
  })";

const std::string kContentGoodJsonResponse = R"({
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
            "link": "https://www.example.co.uk",
            "owner": "John Doe",
            "mime_type": "application/vnd.google-apps.document",
            "updated_time": 1192487100
          }
        },
        "destinationUri": "https://www.example.com",
        "iconUri": "https://example.com/icon.png",
        "score": 0.4,
        "dataStore": "project2"
      }
    ]
  })";

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
            "suggestion": "missingDisplayName@example.com",
            "document": {
              "derivedStructData": {
                "name": {
                  "userName": "missingDisplayName"
                }
              }
            },
            "destinationUri": "https://example.com/people/jdoe"
          },
          {
            "suggestion": "missingUri@example.com",
            "document": {
              "derivedStructData": {
                "name": {
                  "displayName": "John Doe",
                  "userName": "john"
                }
              }
            }
          },
          {
            "document": {
              "derivedStructData": {
                "name": {
                  "displayName": "Missing suggestion / user name / URI"
                }
              }
            }
          },
          {
            "suggestion": "john@example.com",
            "document": {
              "derivedStructData": {
                "name": {
                  "displayName": "John Doe",
                  "userName": "john"
                }
              }
            },
            "destinationUri": "https://example.com/people/jdoe"
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
                "link": "https://www.missingTitle.co.uk"
              }
            },
            "destinationUri": "https://www.missingTitle.com"
          },
          {
            "document": {
              "name": "Document 2",
              "derivedStructData": {
                "title": "John's doodle'",
                "link": "https://www.missinguributlinkavailable.co.uk"
              }
            }
          },
          {
            "document": {
              "name": "Document 3",
              "derivedStructData": {
                "title": "John's doodle'",
                "link": "https://www.example.co.uk"
              }
            },
            "destinationUri": "https://www.example.com"
          }
        ]
        })");

const std::string kGoodJsonResponseImageUrls = base::StringPrintf(
    R"({
      "querySuggestions": [],
      "peopleSuggestions": [
        {
          "suggestion": "john@example.com",
          "document": {
            "name": "sundar",
            "derivedStructData": {
              "name": {
                "familyName": "Doe",
                "givenName": "John",
                "displayName": "John Doe"
              },
              "emails": [
                {
                  "type": "primary",
                  "value": "john@example.com"
                }
              ],
              "displayPhoto": {
                "url": "https://lh3.googleusercontent.com/some/path-s100"
              }
            }
          },
          "destinationUri": "https://example.com/people/jdoe",
          "dataStore": "project 1"
        },
        {
          "suggestion": "john2@example.com",
          "document": {
            "name": "sundar2",
            "derivedStructData": {
              "name": {
                "familyName": "Doe2",
                "givenName": "John",
                "displayName": "John Doe2"
              },
              "emails": [
                {
                  "type": "primary",
                  "value": "john2@example.com"
                }
              ],
              "displayPhoto": {
                "url": "https://lh3.googleusercontent.com/some/path=s100"
              }
            }
          },
          "destinationUri": "https://example.com/people/jdoe2",
          "dataStore": "project 1"
        },
        {
          "suggestion": "john3@example.com",
          "document": {
            "name": "sundar3",
            "derivedStructData": {
              "name": {
                "familyName": "Doe3",
                "givenName": "John",
                "displayName": "John Doe3"
              },
              "emails": [
                {
                  "type": "primary",
                  "value": "john3@example.com"
                }
              ],
              "displayPhoto": {
                "url": "https://lh3.googleusercontent.com/some/path=abc"
              }
            }
          },
          "destinationUri": "https://example.com/people/jdoe3",
          "dataStore": "project 1"
        },
        {
          "suggestion": "john4@example.com",
          "document": {
            "name": "sundar4",
            "derivedStructData": {
              "name": {
                "familyName": "Doe4",
                "givenName": "John",
                "displayName": "John Doe4"
              },
              "emails": [
                {
                  "type": "primary",
                  "value": "john4@example.com"
                }
              ],
              "displayPhoto": {
                "url": "https://lh3.googleusercontent.com/some/path=w100-h200"
              }
            }
          },
          "destinationUri": "https://example.com/people/jdoe4",
          "dataStore": "project 1"
        }
      ],
      "contentSuggestions": []
    })");

const std::string kNonDictJsonResponse =
    base::StringPrintf(R"(["test","result1","result2"])");

// Helper methods to dynamically generate valid responses.
std::string CreateQueryResult(const std::string& query,
                              const float score = 0.0) {
  return base::StringPrintf(
      R"(
        {"suggestion": "%s", "score": %0.1f}
        )",
      query, score);
}
std::string CreatePeopleResult(const std::string& displayName,
                               const std::string& userName,
                               const std::string& givenName,
                               const std::string& familyName,
                               const float score = 0.0) {
  return base::StringPrintf(
      R"(
        {
          "suggestion": "%s@example.com",
          "document": {
            "derivedStructData": {
              "name": {
                "displayName": "%s",
                "givenName": "%s",
                "familyName": "%s"
              },
              "emails": [
                {
                "type": "",
                "value": "%s@example.com"
                }
              ]
            }
          },
          "destinationUri": "https://example.com/people/%s",
          "score": %0.1f
        }
        )",
      userName, displayName, givenName, familyName, userName, userName, score);
}
std::string CreatePeopleResultWithoutEmailField(const std::string& displayName,
                                                const std::string& userName,
                                                const std::string& givenName,
                                                const std::string& familyName,
                                                const float score = 0.0) {
  return base::StringPrintf(
      R"(
        {
          "suggestion": "%s",
          "document": {
            "derivedStructData": {
              "name": {
                "displayName": "%s",
                "givenName": "%s",
                "familyName": "%s"
              }
            }
          },
          "destinationUri": "https://example.com/people/%s",
          "score": %0.1f
        }
        )",
      displayName, displayName, givenName, familyName, userName, score);
}
std::string CreateContentResult(const std::string& title,
                                const std::string& url,
                                const float score = 0.0) {
  return base::StringPrintf(
      R"(
        {
          "document": {
            "derivedStructData": {
              "title": "%s"
            }
          },
          "destinationUri": "%s",
          "score": %0.1f
        }
        )",
      title, url, score);
}

std::string CreateContentResultWithOwnerEmail(const std::string& title,
                                              const std::string& owner_email,
                                              const std::string& url) {
  return base::StringPrintf(
      R"(
        {
          "document": {
            "derivedStructData": {
              "title": "%s",
              "owner_email": "%s"
            }
          },
          "destinationUri": "%s"
        }
        )",
      title, owner_email, url);
}
std::string CreateContentResultWithTypes(const std::string& title,
                                         const std::string& url,
                                         const std::string& mime_type,
                                         const std::string& source_type) {
  return base::StringPrintf(
      R"(
        {
          "document": {
            "derivedStructData": {
              "title": "%s",
              "mime_type": "%s",
              "source_type": "%s"
            }
          },
          "destinationUri": "%s"
        }
        )",
      title, mime_type, source_type, url);
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
  void SetUp() override {
    InitFeature();
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    InitClient();
    mock_listener_ = std::make_unique<
        testing::StrictMock<MockAutocompleteProviderListener>>();
    provider_ = new FakeEnterpriseSearchAggregatorProvider(
        client_.get(), mock_listener_.get());
    InitTemplateUrlService();
  }

  void InitClient() {
    EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
    EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
        .WillRepeatedly(Return(true));
  }

  virtual void InitFeature() {
    scoped_config_.Get().enabled = true;
    scoped_config_.Get().relevance_scoring_mode = "client";
  }

  void InitTemplateUrlService() {
    TemplateURLData data;
    data.SetShortName(u"keyword");
    data.SetKeyword(u"keyword");
    data.SetURL("https://www.google.com/?q={searchTerms}");
    data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
    data.favicon_url = GURL("https://www.google.com/favicon.ico");
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    data.featured_by_policy = true;
    data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
    provider_->template_url_ = client_->GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(data));
  }

  AutocompleteMatch CreateAutocompleteMatch(std::u16string url) {
    AutocompleteMatch match(provider_.get(), 1000, false,
                            AutocompleteMatchType::NAVSUGGEST);
    match.destination_url = GURL(url);
    return match;
  }

  // Returns `destination_url`s of `matches_`.
  std::vector<std::u16string> GetMatches() {
    std::vector<std::u16string> matches;
    for (const auto& m : provider_->matches_)
      matches.push_back(base::UTF8ToUTF16(m.destination_url.spec()));
    return matches;
  }

  // Returns `destination_url`s and `relevance`s of `matches_`.
  using ScoredMatch = std::pair<std::u16string, int>;
  std::vector<ScoredMatch> GetScoredMatches() {
    std::vector<ScoredMatch> matches;
    for (const auto& m : provider_->matches_)
      matches.emplace_back(base::UTF8ToUTF16(m.destination_url.spec()),
                           m.relevance);
    return matches;
  }

  // Simulate requests starting or resetting. Requests whose indexes are
  // included in `allowed_requests` are expected to start and completed or be
  // interrupted. Requests excluded will be reset but not expected to start,
  // complete, or be interrupted. This mirrors the lifecycle of real `Request`s.
  // This acts like calling `Run()` without kicking off the remote service. Does
  // not simulate `Start()`; i.e. skips over provider eligibility checks.
  void StartRequests(std::vector<int> allowed_requests) {
    provider_->done_ = false;

    for (size_t i = 0; i < provider_->requests_.size(); ++i) {
      bool allowed = base::Contains(allowed_requests, i);
      provider_->requests_[i].Reset(!allowed);
      if (allowed) {
        provider_->RequestStarted(i, nullptr);
      }
    }

    EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
        .Times(1);
    provider_->AggregateMatches();
  }

  // Simulate a request completing either successfully, with error, or empty.
  void CompleteRequest(int index, int code, std::string response) {
    base::test::TestFuture<void> future_;
    EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
        .Times(1)
        .WillOnce([&] { future_.SetValue(); });
    provider_->RequestCompleted(index, nullptr, code, std::move(response));
    EXPECT_TRUE(future_.WaitAndClear());
    testing::Mock::VerifyAndClearExpectations(mock_listener_.get());
  }

  // Helper to combine `StartRequests()` and `CompleteRequest()`.
  void StartAndCompleteRequests(
      int response_code,
      std::vector<std::optional<std::string>> response_strings) {
    std::vector<int> allowed_requests;
    for (size_t i = 0; i < response_strings.size(); ++i) {
      if (response_strings[i].has_value()) {
        allowed_requests.push_back(i);
      }
    }

    StartRequests(allowed_requests);

    for (auto i : allowed_requests) {
      CompleteRequest(i, response_code, *response_strings[i]);
    }

    EXPECT_TRUE(provider_->done_);
  }

  // Helper to start and complete all 3 requests with the same response.
  void StartAndComplete3Requests(int response_code,
                                 const std::string& response_string) {
    StartAndCompleteRequests(
        response_code,
        {{response_string}, {response_string}, {response_string}});
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  std::unique_ptr<testing::StrictMock<MockAutocompleteProviderListener>>
      mock_listener_;
  scoped_refptr<FakeEnterpriseSearchAggregatorProvider> provider_;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;
};

// `multiple_requests` is used in the provider's constructor, so it has to be
// setup before the provider is constructed.
class EnterpriseSearchAggregatorProviderSingleRequestTest
    : public EnterpriseSearchAggregatorProviderTest {
  void InitFeature() override {
    EnterpriseSearchAggregatorProviderTest::InitFeature();
    scoped_config_.Get().multiple_requests = false;
  }
};

TEST_F(EnterpriseSearchAggregatorProviderTest, CreateMatch) {
  provider_->adjusted_input_ = CreateInput(u"input", true);
  auto primary_text_class = [&](auto suggestion_type) {
    return suggestion_type ==
                   AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE
               ? std::vector<
                     ACMatchClassification>{{0, ACMatchClassification::NONE}}
               : std::vector<ACMatchClassification>{
                     {0, ACMatchClassification::MATCH},
                     {5, ACMatchClassification::NONE}};
  };
  auto secondary_text_class = [&](auto suggestion_type) {
    return std::vector<ACMatchClassification>{{0, ACMatchClassification::DIM}};
  };

  auto query_match = provider_->CreateMatch(
      AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY, false, {1000},
      "https://url.com/", "https://example.com/image.png",
      "https://example.com/icon.png", u"additional text", u"input title",
      u"keyword input title");
  EXPECT_EQ(query_match.relevance, 1000);
  EXPECT_EQ(query_match.destination_url.spec(), "https://url.com/");
  EXPECT_EQ(query_match.fill_into_edit, u"keyword input title");
  EXPECT_EQ(query_match.enterprise_search_aggregator_type,
            AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY);
  EXPECT_EQ(query_match.description, u"additional text");
  EXPECT_EQ(query_match.description_class,
            secondary_text_class(
                AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY));
  EXPECT_EQ(query_match.contents, u"input title");
  EXPECT_EQ(query_match.contents_class,
            primary_text_class(
                AutocompleteMatch::EnterpriseSearchAggregatorType::QUERY));
  EXPECT_EQ(query_match.image_url.spec(), "https://example.com/image.png");
  EXPECT_EQ(query_match.icon_url.spec(), "https://example.com/icon.png");
  EXPECT_EQ(query_match.keyword, u"keyword");
  EXPECT_TRUE(PageTransitionCoreTypeIs(query_match.transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(query_match.from_keyword, true);
  EXPECT_EQ(query_match.search_terms_args->search_terms, u"input title");

  auto people_match = provider_->CreateMatch(
      AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE, true, {1000},
      "https://url.com/", "https://example.com/image.png",
      "https://example.com/icon.png", u"Keyword People", u"input name",
      u"keyword https://url.com/");
  EXPECT_EQ(people_match.relevance, 1000);
  EXPECT_EQ(people_match.destination_url.spec(), "https://url.com/");
  EXPECT_EQ(people_match.fill_into_edit, u"keyword https://url.com/");
  EXPECT_EQ(people_match.enterprise_search_aggregator_type,
            AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE);
  EXPECT_EQ(people_match.description, u"Keyword People");
  EXPECT_EQ(people_match.description_class,
            primary_text_class(
                AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE));
  EXPECT_EQ(people_match.contents, u"input name");
  EXPECT_EQ(people_match.contents_class,
            secondary_text_class(
                AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE));
  EXPECT_EQ(people_match.image_url.spec(), "https://example.com/image.png");
  EXPECT_EQ(people_match.icon_url.spec(), "https://example.com/icon.png");
  EXPECT_EQ(people_match.keyword, u"keyword");
  EXPECT_TRUE(PageTransitionCoreTypeIs(people_match.transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(people_match.from_keyword, true);
  EXPECT_EQ(people_match.search_terms_args, nullptr);
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
    // The provider is run regardless of default search engine.
    TemplateURLService* turl_service = client_->GetTemplateURLService();
    TemplateURLData data;
    data.SetShortName(u"test");
    data.SetURL("https://www.yahoo.com/?q={searchTerms}");
    data.suggestions_url = "https://www.yahoo.com/complete/?q={searchTerms}";
    TemplateURL* new_default_provider =
        turl_service->Add(std::make_unique<TemplateURL>(data));
    turl_service->SetUserSelectedDefaultSearchProvider(new_default_provider);
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
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

  provider_->done_ = false;
  provider_->Start(invalid_input, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test that a call to `Start()` will clear cached matches and not send a new
// request if input (scoped) is empty.
TEST_F(EnterpriseSearchAggregatorProviderTest,
       StartCallsStopForScopedEmptyInput) {
  provider_->adjusted_input_ = CreateInput(u"keyword", true);
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org/")};

  provider_->Start(provider_->adjusted_input_, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
}

// Test that a call to `Start()` will not send a new request if input is zero
// suggest. This test also checks that both code paths, when `multiple_requests`
// equals true or false, work.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStopForZeroSuggest) {
  AutocompleteInput input = CreateInput(u"", false);
  input.set_focus_type(metrics::INTERACTION_FOCUS);
  provider_->adjusted_input_ = input;
  provider_->matches_ = {CreateAutocompleteMatch(u"https://cached.org/")};

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

  provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

// Test response is parsed accurately.
TEST_F(EnterpriseSearchAggregatorProviderTest, Parse) {
  base::test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  scoped_config_.Get().relevance_scoring_mode = "server";

  provider_->adjusted_input_ = CreateInput(u"john d", true);
  StartAndComplete3Requests(200, kGoodJsonResponse);

  ACMatches matches = provider_->matches_;
  ASSERT_EQ(matches.size(), 3u);

  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[0].relevance, 810);
  EXPECT_EQ(matches[0].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[0].description, u"John Doe");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://example.com/people/jdoe"));
  EXPECT_EQ(matches[0].image_url, GURL("https://example.com/image.png"));
  EXPECT_EQ(matches[0].icon_url, GURL("https://www.google.com/favicon.ico"));
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[0].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[0].fill_into_edit,
            u"keyword https://example.com/people/jdoe");

  EXPECT_EQ(matches[1].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[1].relevance, 410);
  EXPECT_EQ(matches[1].contents, u"10/15/07 - John Doe - Google Docs");
  EXPECT_EQ(matches[1].description, u"John's doodle");
  EXPECT_EQ(matches[1].destination_url, GURL("https://www.example.com"));
  EXPECT_EQ(matches[1].image_url, GURL());
  EXPECT_EQ(matches[1].icon_url, GURL("https://example.com/icon.png"));
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[1].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[1].fill_into_edit, u"keyword https://www.example.com");

  EXPECT_EQ(matches[2].type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(matches[2].relevance, 0);
  EXPECT_EQ(matches[2].contents, u"John's Document 1");
  EXPECT_EQ(matches[2].description, u"");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://www.google.com/?q=John%27s+Document+1"));
  EXPECT_EQ(matches[2].image_url, GURL());
  EXPECT_EQ(matches[2].icon_url, GURL());
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[2].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[2].fill_into_edit, u"keyword John's Document 1");
}

// Test response is parsed accurately.
TEST_F(EnterpriseSearchAggregatorProviderTest, Parse_MultipleResponses) {
  base::test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  scoped_config_.Get().relevance_scoring_mode = "server";

  provider_->adjusted_input_ = CreateInput(u"john d", true);
  StartAndComplete3Requests(200, kGoodJsonResponse);

  ACMatches matches = provider_->matches_;
  ASSERT_EQ(matches.size(), 3u);

  EXPECT_EQ(matches[0].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[0].relevance, 810);
  EXPECT_EQ(matches[0].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[0].description, u"John Doe");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://example.com/people/jdoe"));
  EXPECT_EQ(matches[0].image_url, GURL("https://example.com/image.png"));
  EXPECT_EQ(matches[0].icon_url, GURL("https://www.google.com/favicon.ico"));
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[0].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[0].fill_into_edit,
            u"keyword https://example.com/people/jdoe");

  EXPECT_EQ(matches[1].type, AutocompleteMatchType::NAVSUGGEST);
  EXPECT_EQ(matches[1].relevance, 410);
  EXPECT_EQ(matches[1].contents, u"10/15/07 - John Doe - Google Docs");
  EXPECT_EQ(matches[1].description, u"John's doodle");
  EXPECT_EQ(matches[1].destination_url, GURL("https://www.example.com"));
  EXPECT_EQ(matches[1].image_url, GURL());
  EXPECT_EQ(matches[1].icon_url, GURL("https://example.com/icon.png"));
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[1].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[1].fill_into_edit, u"keyword https://www.example.com");

  EXPECT_EQ(matches[2].type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(matches[2].relevance, 0);
  EXPECT_EQ(matches[2].contents, u"John's Document 1");
  EXPECT_EQ(matches[2].description, u"");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://www.google.com/?q=John%27s+Document+1"));
  EXPECT_EQ(matches[2].image_url, GURL());
  EXPECT_EQ(matches[2].icon_url, GURL());
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[2].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(matches[2].fill_into_edit, u"keyword John's Document 1");
}

// Test response is parsed accurately and image_url is adjusted appropriately.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseAndModifyImageUrls) {
  provider_->adjusted_input_ = CreateInput(u"john d", true);
  StartAndComplete3Requests(200, kGoodJsonResponseImageUrls);

  ACMatches matches = provider_->matches_;
  ASSERT_EQ(matches.size(), 4u);

  EXPECT_EQ(matches[0].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[0].description, u"John Doe");
  EXPECT_EQ(matches[0].image_url,
            GURL("https://lh3.googleusercontent.com/some/path-s100=s64"));

  EXPECT_EQ(matches[1].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[1].description, u"John Doe2");
  EXPECT_EQ(matches[1].image_url,
            GURL("https://lh3.googleusercontent.com/some/path=s100"));

  EXPECT_EQ(matches[2].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[2].description, u"John Doe3");
  EXPECT_EQ(matches[2].image_url,
            GURL("https://lh3.googleusercontent.com/some/path=abc-s64"));

  EXPECT_EQ(matches[3].contents,
            l10n_util::GetStringFUTF16(IDS_PERSON_SUGGESTION_DESCRIPTION,
                                       u"keyword"));
  EXPECT_EQ(matches[3].description, u"John Doe4");
  EXPECT_EQ(matches[3].image_url,
            GURL("https://lh3.googleusercontent.com/some/path=w100-h200"));
}

// Test results with missing expected fields are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithMissingFields) {
  provider_->adjusted_input_ = CreateInput(u"john d", true);
  StartAndComplete3Requests(200, kMissingFieldsJsonResponse);
  EXPECT_THAT(
      GetMatches(),
      testing::ElementsAre(u"https://example.com/people/jdoe",
                           u"https://www.example.com/",
                           u"https://www.google.com/?q=John%27s+Document+1"));
}

// Test non-dict results are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, ParseWithNonDict) {
  AutocompleteInput input = CreateInput(u"john d", true);
  provider_->adjusted_input_ = input;

  StartAndComplete3Requests(200, kNonDictJsonResponse);
  EXPECT_THAT(GetMatches(), testing::ElementsAre());
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

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(1);
  provider_->Start(input, false);
  StartAndComplete3Requests(200, kGoodJsonResponse);
  EXPECT_EQ(provider_->matches_[0].keyword, u"unfeatured");
  EXPECT_THAT(GetMatches(), testing::ElementsAre(
                                u"https://example.com/people/jdoe",
                                u"https://www.example.com/",
                                u"http://www.yahoo.com/John's%20Document%201"));
}

// Test things work in unscoped mode and query suggestions are skipped.
TEST_F(EnterpriseSearchAggregatorProviderTest, UnscopedMode) {
  AutocompleteInput input = CreateInput(u"john d", false);

  EXPECT_CALL(*mock_listener_.get(), OnProviderUpdate(_, provider_.get()))
      .Times(1);
  provider_->Start(input, false);
  StartAndCompleteRequests(200, {{}, kGoodJsonResponse, kGoodJsonResponse});
  EXPECT_THAT(GetMatches(),
              testing::ElementsAre(u"https://example.com/people/jdoe",
                                   u"https://www.example.com/"));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, Limits) {
  // At most 2 per type when unscoped. Filtered matches shouldn't count against
  // the limit.
  provider_->adjusted_input_ = CreateInput(u"mango m", false);
  StartAndComplete3Requests(
      200,
      CreateResponse(
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
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/mango-1-people", 607},
          ScoredMatch{u"https://example.com/people/mango-2-people", 606},
          ScoredMatch{u"https://url-mango-1/", 517},
          ScoredMatch{u"https://url-mango-2/", 516},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 507},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 506}));

  // At most 4 per type when scoped. Filtered matches shouldn't count against
  // the limit.
  provider_->adjusted_input_ = CreateInput(u"mango m", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {
              CreateQueryResult("grape-1-query"),
              CreateQueryResult("grape-2-query"),
              CreateQueryResult("grape-3-query"),
              CreateQueryResult("mango-1-query"),
              CreateQueryResult("mango-2-query"),
              CreateQueryResult("mango-3-query"),
              CreateQueryResult("mango-4-query"),
              CreateQueryResult("mango-5-query"),
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
              CreatePeopleResult("displayName", "mango-4-people", "givenName",
                                 "familyName"),
              CreatePeopleResult("displayName", "mango-5-people", "givenName",
                                 "familyName"),
          },
          {
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
              CreateContentResult("mango-4-content", "https://url-mango-4/"),
              CreateContentResult("mango-5-content", "https://url-mango-5/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/mango-1-people", 607},
          ScoredMatch{u"https://example.com/people/mango-2-people", 606},
          ScoredMatch{u"https://example.com/people/mango-3-people", 605},
          ScoredMatch{u"https://example.com/people/mango-4-people", 604},
          ScoredMatch{u"https://url-mango-1/", 517},
          ScoredMatch{u"https://url-mango-2/", 516},
          ScoredMatch{u"https://url-mango-3/", 515},
          ScoredMatch{u"https://url-mango-4/", 514},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 507},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 506},
          ScoredMatch{u"https://www.google.com/?q=mango-3-query", 505},
          ScoredMatch{u"https://www.google.com/?q=mango-4-query", 504}));

  // Types that have less than 2 results aren't backfilled by other types.
  provider_->adjusted_input_ = CreateInput(u"mango m", false);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {}, {},
          {
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-mango-1/", 517},
                                   ScoredMatch{u"https://url-mango-2/", 516}));

  // The best 2 suggestions should be shown, even if they're not
  // the 1st 2.
  provider_->adjusted_input_ = CreateInput(u"mango mango-2 mango-3", false);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {}, {},
          {
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-mango-2/", 519},
                                   ScoredMatch{u"https://url-mango-3/", 518}));

  // Can show more than 2 per type when scoped.
  provider_->adjusted_input_ = CreateInput(u"mango m", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
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
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/mango-1-people", 607},
          ScoredMatch{u"https://example.com/people/mango-2-people", 606},
          ScoredMatch{u"https://example.com/people/mango-3-people", 605},
          ScoredMatch{u"https://url-mango-1/", 517},
          ScoredMatch{u"https://url-mango-2/", 516},
          ScoredMatch{u"https://url-mango-3/", 515},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 507},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 506},
          ScoredMatch{u"https://www.google.com/?q=mango-3-query", 505}));

  // Limit low-quality suggestions. Only the 1st 2 matches are allowed to score
  // lower than 500. Even if the 1st 2 matches score higher than 500, the
  // remaining matches must also score higher than 500.
  provider_->adjusted_input_ = CreateInput(u"m ma", false);
  StartAndComplete3Requests(
      200,
      CreateResponse(
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
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/mango-1-people", 307},
          ScoredMatch{u"https://example.com/people/mango-2-people", 306}));

  // Scoped inputs have a higher limit of 8 matches allowed to score lower than
  // 500. Even if the 1st 2 matches score higher than 500, the remaining matches
  // must also score higher than 500.
  provider_->adjusted_input_ = CreateInput(u"m ma", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
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
              CreateContentResult("grape-1-content", "https://url-grape-1/"),
              CreateContentResult("grape-2-content", "https://url-grape-2/"),
              CreateContentResult("grape-3-content", "https://url-grape-3/"),
              CreateContentResult("mango-1-content", "https://url-mango-1/"),
              CreateContentResult("mango-2-content", "https://url-mango-2/"),
              CreateContentResult("mango-3-content", "https://url-mango-3/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/mango-1-people", 307},
          ScoredMatch{u"https://example.com/people/mango-2-people", 306},
          ScoredMatch{u"https://example.com/people/mango-3-people", 305},
          ScoredMatch{u"https://url-mango-1/", 217},
          ScoredMatch{u"https://url-mango-2/", 216},
          ScoredMatch{u"https://url-mango-3/", 215},
          ScoredMatch{u"https://www.google.com/?q=mango-1-query", 207},
          ScoredMatch{u"https://www.google.com/?q=mango-2-query", 206}));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, ServerRelevanceScoring) {
  scoped_config_.Get().relevance_scoring_mode = "server";

  provider_->adjusted_input_ = CreateInput(u"match m", true);
  StartAndComplete3Requests(
      200, CreateResponse(
               {
                   CreateQueryResult("matchQuery", 1.0),
                   // Results that don't match the input should still be scored
                   // and returned.
                   CreateQueryResult("query", 1.0),
                   CreateQueryResult("query2", 1.0),
                   CreateQueryResult("query3", 0.8),
               },
               {
                   CreatePeopleResult("displayName", "matchUserName",
                                      "givenName", "familyName", 0.7),
                   // A result with a score of 0 should not be returned.
                   CreatePeopleResult("displayName", "userName", "givenName",
                                      "familyName", 0.0),
               },
               {
                   CreateContentResult("matchTitle", "https://url/", 0.7),
                   CreateContentResult("title", "https://url2/", 0.7),
                   CreateContentResult("title2", "https://url3/", 0.3),
               }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://www.google.com/?q=matchQuery", 1010},
          ScoredMatch{u"https://www.google.com/?q=query", 1009},
          ScoredMatch{u"https://www.google.com/?q=query2", 1008},
          ScoredMatch{u"https://www.google.com/?q=query3", 807},
          ScoredMatch{u"https://example.com/people/matchUserName", 710},
          ScoredMatch{u"https://url/", 710}, ScoredMatch{u"https://url2/", 709},
          ScoredMatch{u"https://url3/", 308}));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, MixedRelevanceScoring) {
  scoped_config_.Get().relevance_scoring_mode = "mixed";

  std::string response = CreateResponse(
      {
          CreateQueryResult("matchQuery", 1.0),
          CreateQueryResult("query", 1.0),
      },
      {
          CreatePeopleResult("displayName", "matchUserName", "givenName",
                             "familyName", 0.7),
          CreatePeopleResult("displayName", "userName", "givenName",
                             "familyName", 0.6),
      },
      {
          CreateContentResult("matchTitle", "https://url/", 0.7),
          CreateContentResult("title2", "https://url2/", 0.3),
      });

  // Scoped mode should use server-provided relevance scores.
  provider_->adjusted_input_ = CreateInput(u"match m", true);
  StartAndComplete3Requests(200, response);
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://www.google.com/?q=matchQuery", 1010},
                  ScoredMatch{u"https://www.google.com/?q=query", 1009},
                  ScoredMatch{u"https://example.com/people/matchUserName", 710},
                  ScoredMatch{u"https://url/", 710},
                  ScoredMatch{u"https://example.com/people/userName", 609},
                  ScoredMatch{u"https://url2/", 309}));

  // Unscoped mode should use client-calculated relevance scores.
  provider_->adjusted_input_ = CreateInput(u"match m", false);
  StartAndComplete3Requests(200, response);
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://example.com/people/matchUserName", 610},
                  ScoredMatch{u"https://url/", 520},
                  ScoredMatch{u"https://www.google.com/?q=matchQuery", 510}));
}

TEST_F(EnterpriseSearchAggregatorProviderTest, LocalRelevanceScoring) {
  // Results that don't match the input should be filtered out.
  provider_->adjusted_input_ = CreateInput(u"match m", true);
  StartAndComplete3Requests(
      200, CreateResponse(
               {
                   CreateQueryResult("query"),
                   CreateQueryResult("matchQuery"),
               },
               {
                   CreatePeopleResult("displayName", "userName", "givenName",
                                      "familyName"),
                   CreatePeopleResult("displayName", "matchUserName",
                                      "givenName", "familyName"),
               },
               {
                   CreateContentResult("title", "https://url/"),
                   CreateContentResult("matchTitle", "https://url/"),
               }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://example.com/people/matchUserName", 609},
                  ScoredMatch{u"https://url/", 519},
                  ScoredMatch{u"https://www.google.com/?q=matchQuery", 509},
                  FieldsAre(_, 0), FieldsAre(_, 0), FieldsAre(_, 0)));

  // Score using weighted sum of matches.
  provider_->adjusted_input_ = CreateInput(u"zero on tw th", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {}, {},
          {
              CreateContentResult("zero", "https://url-0/"),
              CreateContentResult("zero one", "https://url-01/"),
              CreateContentResult("zero one two", "https://url-012/"),
              CreateContentResult("zero one two three", "https://url-0123/"),
          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-0123/", 717},
                                   ScoredMatch{u"https://url-012/", 618},
                                   ScoredMatch{u"https://url-01/", 519},
                                   FieldsAre(_, 0)));

  // Duplicate matches do not count.
  // - If the input repeats a word, only 1 should count.
  // - If the result field repeats a word, only 1 should count.
  // - If a word appears in multiple result fields, only 1 should count.
  provider_->adjusted_input_ = CreateInput(u"one one", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResultWithOwnerEmail(
                                  "one one", "one one", "https://url-1/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-1/", 420}));

  // Each input word can match only 1 result word.
  provider_->adjusted_input_ = CreateInput(u"one one", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResult("one oneTwo", "https://url/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 420}));

  // A result word can match multiple input words. This is just a side effect
  // of the implementation rather than intentional design.
  provider_->adjusted_input_ = CreateInput(u"one on o", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResult("one", "https://url/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 620}));

  // Matches outside contents and description contribute less to the score.
  provider_->adjusted_input_ = CreateInput(u"one two three four five", true);
  StartAndComplete3Requests(
      200, CreateResponse(
               {}, {},
               {
                   CreateContentResultWithOwnerEmail(
                       "title one", "two three four five", "https://inside/"),
                   CreateContentResultWithOwnerEmail(
                       "title", "one two three four five", "https://outside/"),
               }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://inside/", 820},
                                   ScoredMatch{u"https://outside/", 519}));

  // Short input words contribute less to the score.
  provider_->adjusted_input_ = CreateInput(u"on two three four five", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResultWithOwnerEmail(
                                  "one", "two three four five", "https://url/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 520}));

  // Short input words contribute less to score, except for exact (non-prefix)
  // matches in people suggestions.
  provider_->adjusted_input_ = CreateInput(u"weak ab", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {},
          {
              {CreatePeopleResultWithoutEmailField("ab", "ab", "weak", "")},
              {CreatePeopleResultWithoutEmailField("abc", "abc", "weak", "")},
          },
          {
              CreateContentResultWithOwnerEmail("ab", "weak",
                                                "https://url-ab/"),
              CreateContentResultWithOwnerEmail("abc", "weak",
                                                "https://url-abc/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(ScoredMatch{u"https://example.com/people/ab", 610},
                           ScoredMatch{u"https://example.com/people/abc", 309},
                           ScoredMatch{u"https://url-ab/", 220},
                           ScoredMatch{u"https://url-abc/", 219}));

  // For all suggestions, long input words contribute fully to the score
  // regardless of whether they fully or prefix match.
  provider_->adjusted_input_ = CreateInput(u"weak abc", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {},
          {
              {CreatePeopleResultWithoutEmailField("abc", "abc", "weak", "")},
              {CreatePeopleResultWithoutEmailField("abcd", "abcd", "weak", "")},
          },
          {
              CreateContentResultWithOwnerEmail("abc", "weak",
                                                "https://url-abc/"),
              CreateContentResultWithOwnerEmail("abcd", "weak",
                                                "https://url-abcd/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(ScoredMatch{u"https://example.com/people/abc", 610},
                           ScoredMatch{u"https://example.com/people/abcd", 609},
                           ScoredMatch{u"https://url-abc/", 520},
                           ScoredMatch{u"https://url-abcd/", 519}));

  // Matches outside human-readable fields (e.g. URL) aren't considered in
  // scoring.
  (provider_->adjusted_input_) = CreateInput(u"title url", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResult("title", "https://url1/"),
                              CreateContentResult("title", "https://url2/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url1/", 420},
                                   ScoredMatch{u"https://url2/", 419}));

  // Suggestions that match every input words, when there are at least 2, should
  // be scored higher.
  (provider_->adjusted_input_) = CreateInput(u"one two", true);
  StartAndComplete3Requests(
      200,
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "https://url/"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 1020}));

  // Suggestions that match every input words, when there is not at least 2,
  // should not be scored higher.
  (provider_->adjusted_input_) = CreateInput(u"one", true);
  StartAndComplete3Requests(
      200,
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "https://url/"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 420}));

  // Suggestions that match at least 2 but not all inputs words should not be
  // scored higher.
  (provider_->adjusted_input_) = CreateInput(u"one two four", true);
  StartAndComplete3Requests(
      200,
      CreateResponse({}, {},
                     {
                         CreateContentResult("one two three", "https://url/"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 820}));

  // Require at least 1 strong match or 2 weak matches.
  (provider_->adjusted_input_) = CreateInput(u"title", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResult("title", "https://url/"),
                          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url/", 420}));

  // When unscoped, requires at least 1 strong match or 2 weak matches.
  (provider_->adjusted_input_) = CreateInput(u"user gmail", false);
  StartAndComplete3Requests(
      200,
      CreateResponse({}, {},
                     {
                         CreateContentResultWithOwnerEmail(
                             "title", "user@example.com", "https://url-1/"),
                         CreateContentResultWithOwnerEmail(
                             "title", "user@gmail.com", "https://url-2/"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-2/", 219},
                                   FieldsAre(_, 0)));

  // When scoped, does not require at least 1 strong match or 2 weak matches.
  (provider_->adjusted_input_) = CreateInput(u"user gmail", true);
  StartAndComplete3Requests(
      200,
      CreateResponse({}, {},
                     {
                         CreateContentResultWithOwnerEmail(
                             "title", "user@example.com", "https://url-1/"),
                         CreateContentResultWithOwnerEmail(
                             "title", "user@gmail.com", "https://url-2/"),
                     }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(ScoredMatch{u"https://url-2/", 219},
                                   ScoredMatch{u"https://url-1/", 120}));

  // Require at least half the input words to match.
  provider_->adjusted_input_ = CreateInput(u"title x y", true);
  StartAndComplete3Requests(
      200, CreateResponse({}, {},
                          {
                              CreateContentResult("title", "https://url/"),
                          }));
  EXPECT_THAT(GetScoredMatches(), testing::ElementsAre(FieldsAre(_, 0)));

  // People matches should be boosted. Also test that people matches without
  // email fields are still scored appropriately.
  provider_->adjusted_input_ = CreateInput(u"input i", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {
              CreateQueryResult("input"),
          },
          {
              CreatePeopleResult("displayName input", "userName", "givenName",
                                 "familyName"),
              CreatePeopleResult("displayName", "NoMatchUserName", "givenName",
                                 "familyName"),
              CreatePeopleResultWithoutEmailField(
                  "displayName input", "userName2", "givenName", "familyName"),
              CreatePeopleResultWithoutEmailField(
                  "displayName", "NoMatchUserName2", "givenName", "familyName"),
          },
          {
              CreateContentResult("title input", "https://url/"),
          }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://example.com/people/userName", 610},
                  ScoredMatch{u"https://example.com/people/userName2", 608},
                  ScoredMatch{u"https://url/", 520},
                  ScoredMatch{u"https://www.google.com/?q=input", 510},
                  FieldsAre(_, 0), FieldsAre(_, 0)));

  // People matches with exact email matches should be boosted.
  provider_->adjusted_input_ = CreateInput(u"userName d", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {
              CreateQueryResult("userName"),
          },
          {
              CreatePeopleResult("userName displayName", "userName",
                                 "givenName", "familyName"),
              CreatePeopleResult("userName displayName", "userNamePrefixMatch",
                                 "givenName", "familyName"),
              CreatePeopleResult("userName displayName", "NoMatchUserName",
                                 "givenName", "familyName"),
          },
          {
              CreateContentResult("title userName", "https://url/"),
          }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(
          ScoredMatch{u"https://example.com/people/userName", 1010},
          ScoredMatch{u"https://example.com/people/userNamePrefixMatch", 609},
          ScoredMatch{u"https://example.com/people/NoMatchUserName", 608},
          ScoredMatch{u"https://url/", 420},
          ScoredMatch{u"https://www.google.com/?q=userName", 410}));

  // People matches must match all input words.
  provider_->adjusted_input_ = CreateInput(u"query q unmatched", true);
  StartAndComplete3Requests(
      200, CreateResponse(
               {
                   CreateQueryResult("query"),
               },
               {
                   CreatePeopleResult("displayName query", "userName",
                                      "givenName", "familyName"),
                   CreatePeopleResult("displayName", "matchUserName",
                                      "givenName", "familyName"),
               },
               {
                   CreateContentResult("title query", "https://url/"),
               }));
  EXPECT_THAT(
      GetScoredMatches(),
      testing::ElementsAre(ScoredMatch{u"https://url/", 520},
                           ScoredMatch{u"https://www.google.com/?q=query", 510},
                           FieldsAre(_, 0), FieldsAre(_, 0)));

  // When content and query matches equally match the input, content matches
  // should be preferred.
  provider_->adjusted_input_ = CreateInput(u"query", true);
  StartAndComplete3Requests(
      200, CreateResponse(
               {
                   CreateQueryResult("query"),
               },
               {},
               {
                   CreateContentResult("query", "https://url/"),
               }));
  EXPECT_THAT(GetScoredMatches(),
              testing::ElementsAre(
                  ScoredMatch{u"https://url/", 420},
                  ScoredMatch{u"https://www.google.com/?q=query", 410}));
}

TEST_F(EnterpriseSearchAggregatorProviderTest,
       ContentSuggestionTypeDescriptions) {
  provider_->adjusted_input_ = CreateInput(u"input", true);
  StartAndComplete3Requests(
      200,
      CreateResponse(
          {}, {},
          {
              // Verifies use of MIME type.
              CreateContentResultWithTypes(
                  "Evolution of Dance", "https://url1/", "video/quicktime", ""),
              // Verifies use of source type.
              CreateContentResultWithTypes("Uh oh", "https://url2/", "",
                                           "buganizer"),
              // Verifies that MIME type takes precedent over source type.
              CreateContentResultWithTypes(
                  "Same thing we do every night, Pinky", "https://url3/",
                  "image/png", "salesforce"),
          }));
  ACMatches matches = provider_->matches_;
  ASSERT_EQ(matches.size(), 3u);

  // Verifies use of MIME type.
  EXPECT_EQ(matches[0].contents, u"QuickTime Video");
  EXPECT_EQ(matches[0].description, u"Evolution of Dance");
  EXPECT_EQ(matches[0].destination_url, GURL("https://url1/"));

  // Verifies use of source type.
  EXPECT_EQ(matches[1].contents, u"Buganizer Issue");
  EXPECT_EQ(matches[1].description, u"Uh oh");
  EXPECT_EQ(matches[1].destination_url, GURL("https://url2/"));

  // Verifies that MIME type takes precedent over source type.
  EXPECT_EQ(matches[2].contents, u"PNG Image");
  EXPECT_EQ(matches[2].description, u"Same thing we do every night, Pinky");
  EXPECT_EQ(matches[2].destination_url, GURL("https://url3/"));
}

TEST_F(EnterpriseSearchAggregatorProviderSingleRequestTest,
       Logging_SingleRequest) {
  // The code flow is:
  // 1) `Start()`
  // 2) `Run()` is invoked from `Start()` after a potential debouncing.
  // 3) A request is asyncly made to Vertex AI backend once auth token is ready.
  // 4) A response is asyncly received from the Vertex AI backend.
  // At any point, the chain of events can be interrupted by a `Stop()`
  // invocation; usually when there's a new input. The below 3 cases test the
  // logged histograms when `Stop()` is invoked after steps 2, 3, and after the
  // request is completed.

  {
    SCOPED_TRACE("Case: Stop() before Run().");
    ExpectedHistogram expected_histogram{};
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Stop() before request started.");
    ExpectedHistogram expected_histogram{};
    provider_->requests_[0].Reset(true);
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Stop() before response.");
    ExpectedHistogram expected_histogram{.unsliced_interrupted = true};
    StartRequests({0});
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Request complete");
    ExpectedHistogram expected_histogram{.unsliced_completed = true,
                                         .unsliced_count = 3};
    StartAndCompleteRequests(200, {{kGoodJsonResponse}});
  }
}

TEST_F(EnterpriseSearchAggregatorProviderTest, Logging_MultipleRequests) {
  // The code flow is:
  // 1) `Start()`
  // 2) `Run()` is invoked from `Start()` after a potential debouncing.
  // 3) A request is asyncly made to Vertex AI backend once auth token is ready.
  // 4) A response is asyncly received from the Vertex AI backend.
  // At any point, the chain of events can be interrupted by a `Stop()`
  // invocation; usually when there's a new input. The below 3 cases test the
  // logged histograms when `Stop()` is invoked after steps 2, 3, and after the
  // request is completed.

  // Even though this test is testing when `multiple_requests` is enabled, the 3
  // cases below have 1 request starting and/or completing.

  {
    SCOPED_TRACE("Case: Stop() before Run().");
    ExpectedHistogram expected_histogram{};
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Stop() before request started.");
    ExpectedHistogram expected_histogram{};
    provider_->requests_[0].Reset(false);
    provider_->requests_[1].Reset(true);
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Stop() before response.");
    ExpectedHistogram expected_histogram{.unsliced_interrupted = true,
                                         .query_interrupted = true};
    StartRequests({0});
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }

  {
    SCOPED_TRACE("Case: Request complete");
    ExpectedHistogram expected_histogram{
        .unsliced_completed = true,
        .query_completed = true,
        .people_completed = true,
        .content_completed = true,
        .unsliced_count = 3,
        .query_count = 1,
        .people_count = 1,
        .content_count = 1,
    };
    StartAndComplete3Requests(200, kGoodJsonResponse);
  }

  {
    SCOPED_TRACE("Case: Some requests complete");
    ExpectedHistogram expected_histogram{.content_completed = true,
                                         .content_count = 1};
    StartRequests({1, 2});  // Start people & content requests.
    CompleteRequest(2, 200, kGoodJsonResponse);  // Complete content request.
    EXPECT_FALSE(provider_->done_);
    expected_histogram.Verify();

    expected_histogram.unsliced_interrupted = true;
    expected_histogram.people_interrupted = true;
    provider_->Stop(AutocompleteStopReason::kClobbered);
  }
}

TEST_F(EnterpriseSearchAggregatorProviderTest,
       Caching_Logging_MultipleRequests) {
  {
    SCOPED_TRACE("1st autocomplete pass to populate query and people caches");
    // Use a keyword input to avoid filtering low quality matches.
    provider_->adjusted_input_ = CreateInput(u"input", true);
    ExpectedHistogram expected_histogram{};

    {
      SCOPED_TRACE("Start all 3 requests");
      StartRequests({0, 1, 2});
      EXPECT_THAT(GetMatches(), testing::ElementsAre());
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve people request");
      expected_histogram.people_completed = true;
      expected_histogram.people_count = 1;
      CompleteRequest(
          1, 200,
          CreateResponse({},
                         {CreatePeopleResult("Justin Timberlake", "JT",
                                             "Justin", "Timberlake")},
                         {}));
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://example.com/people/JT"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve query request");
      expected_histogram.query_completed = true;
      expected_histogram.query_count = 1;
      CompleteRequest(
          0, 200,
          {{CreateResponse({CreateQueryResult("Prince of Pop")}, {}, {})}});
      EXPECT_THAT(GetMatches(), testing::ElementsAre(
                                    u"https://www.google.com/?q=Prince+of+Pop",
                                    u"https://example.com/people/JT"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve content request");
      expected_histogram.unsliced_completed = true;
      expected_histogram.content_completed = true;
      expected_histogram.unsliced_count = 3;
      expected_histogram.content_count = 1;
      CompleteRequest(
          2, 200,
          CreateResponse({}, {},
                         {CreateContentResult(
                             "My Love", "https://youtube.com/q=my+love")}));
      EXPECT_THAT(GetMatches(), testing::ElementsAre(
                                    u"https://www.google.com/?q=Prince+of+Pop",
                                    u"https://example.com/people/JT",
                                    u"https://youtube.com/q=my+love"));
      EXPECT_TRUE(provider_->done_);
      expected_histogram.Verify();
    }
  }

  {
    SCOPED_TRACE("2nd autocomplete pass to use and clear caches");
    ExpectedHistogram expected_histogram{};

    {
      SCOPED_TRACE("Start people and content requests");
      StartRequests({1, 2});
      // Should still show cached people and content matches. Should clear
      // cached query matches since we know there won't be new query matches.
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://example.com/people/JT",
                                       u"https://youtube.com/q=my+love"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve people request");
      expected_histogram.people_completed = true;
      expected_histogram.people_count = 1;
      CompleteRequest(
          1, 200,
          CreateResponse(
              {},
              {CreatePeopleResult("Chris Brown", "Breezy", "Chris", "Brown")},
              {}));
      // Should not show stale people match since new people matches are
      // available. Should still show cached content match.
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://example.com/people/Breezy",
                                       u"https://youtube.com/q=my+love"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve content request");
      // Should log completed even though query request was not completed
      // because it was not started to begin with.
      expected_histogram.unsliced_completed = true;
      expected_histogram.content_completed = true;
      expected_histogram.unsliced_count = 2;
      expected_histogram.content_count = 1;
      CompleteRequest(2, 200,
                      CreateResponse({}, {},
                                     {CreateContentResult(
                                         "Gimme That",
                                         "https://youtube.com/q=gimme+that")}));
      // Should show no stale matches.
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://example.com/people/Breezy",
                                       u"https://youtube.com/q=gimme+that"));
      EXPECT_TRUE(provider_->done_);
      expected_histogram.Verify();
    }
  }

  {
    SCOPED_TRACE("3rd autocomplete pass, empty and bad responses clear cache");
    ExpectedHistogram expected_histogram{};

    {
      SCOPED_TRACE("Start people and content requests");
      StartRequests({1, 2});
      // Should still show cached people and content matches.
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://example.com/people/Breezy",
                                       u"https://youtube.com/q=gimme+that"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve empty people request");
      expected_histogram.people_completed = true;
      expected_histogram.people_count = 0;
      CompleteRequest(1, 200, CreateResponse({}, {}, {}));
      // Should clear people cache.
      EXPECT_THAT(GetMatches(),
                  testing::ElementsAre(u"https://youtube.com/q=gimme+that"));
      EXPECT_FALSE(provider_->done_);
      expected_histogram.Verify();
    }

    {
      SCOPED_TRACE("Resolve content request with 404");
      // Should log completed even though query request was not completed
      // because it was not started to begin with.
      expected_histogram.unsliced_completed = true;
      expected_histogram.content_completed = true;
      expected_histogram.unsliced_count = 0;
      expected_histogram.content_count = 0;
      CompleteRequest(2, 404,
                      CreateResponse({}, {},
                                     {CreateContentResult(
                                         "Gimme That",
                                         "https://youtube.com/q=gimme+that")}));
      EXPECT_THAT(GetMatches(), testing::ElementsAre());
      EXPECT_TRUE(provider_->done_);
      expected_histogram.Verify();
    }
  }
}
