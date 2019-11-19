// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/entropy_provider.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::IsEmpty;
using testing::Not;
using testing::NotNull;
using testing::Property;
using testing::StartsWith;

const char kAPIKey[] = "fakeAPIkey";
const char kTestEmail[] = "foo@bar.com";
const char kFetchSuggestionsEndpoint[] =
    "https://chromefeedcontentsuggestions-pa.googleapis.com/v2/suggestions/"
    "fetch";

// Artificial time delay for JSON parsing.
const int64_t kTestJsonParsingLatencyMs = 20;

ACTION_P(MoveArgument1PointeeTo, ptr) {
  *ptr = std::move(*arg1);
}

MATCHER(IsNullCategoriesList, "is a null list of categories") {
  RemoteSuggestionsFetcher::OptionalFetchedCategories& fetched_categories =
      *arg;
  return !fetched_categories.has_value();
}

MATCHER(IsEmptyCategoriesList, "is an empty list of categories") {
  RemoteSuggestionsFetcher::OptionalFetchedCategories& fetched_categories =
      *arg;
  return fetched_categories && fetched_categories->empty();
}

MATCHER(IsEmptyArticleList, "is an empty list of articles") {
  RemoteSuggestionsFetcher::OptionalFetchedCategories& fetched_categories =
      *arg;
  return fetched_categories && fetched_categories->size() == 1 &&
         fetched_categories->begin()->suggestions.empty();
}

MATCHER_P(IsSingleArticle, url, "is a list with the single article %(url)s") {
  RemoteSuggestionsFetcher::OptionalFetchedCategories& fetched_categories =
      *arg;
  if (!fetched_categories) {
    *result_listener << "got empty categories.";
    return false;
  }
  if (fetched_categories->size() != 1) {
    *result_listener << "expected single category.";
    return false;
  }
  auto category = fetched_categories->begin();
  if (category->suggestions.size() != 1) {
    *result_listener << "expected single snippet, got: "
                     << category->suggestions.size();
    return false;
  }
  if (category->suggestions[0]->url().spec() != url) {
    *result_listener << "unexpected url, got: "
                     << category->suggestions[0]->url().spec();
    return false;
  }
  return true;
}

MATCHER(IsCategoryInfoForArticles, "") {
  if (arg.additional_action() != ContentSuggestionsAdditionalAction::FETCH) {
    *result_listener << "missing expected FETCH action";
    return false;
  }
  if (!arg.show_if_empty()) {
    *result_listener << "missing expected show_if_empty";
    return false;
  }
  return true;
}

MATCHER_P(FirstCategoryHasInfo, info_matcher, "") {
  if (!arg->has_value() || arg->value().size() == 0) {
    *result_listener << "No category found.";
  }
  return testing::ExplainMatchResult(info_matcher, arg->value().front().info,
                                     result_listener);
}

class MockSnippetsAvailableCallback {
 public:
  // Workaround for gMock's lack of support for movable arguments.
  void WrappedRun(
      Status status,
      RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories) {
    Run(status, &fetched_categories);
  }

  MOCK_METHOD2(Run,
               void(Status status,
                    RemoteSuggestionsFetcher::OptionalFetchedCategories*
                        fetched_categories));
};

void ParseJson(const std::string& json,
               SuccessCallback success_callback,
               ErrorCallback error_callback) {
  base::JSONReader json_reader;
  base::Optional<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    std::move(success_callback).Run(std::move(*value));
  } else {
    std::move(error_callback).Run(json_reader.GetErrorMessage());
  }
}

void ParseJsonDelayed(const std::string& json,
                      SuccessCallback success_callback,
                      ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParseJson, json, std::move(success_callback),
                     std::move(error_callback)),
      base::TimeDelta::FromMilliseconds(kTestJsonParsingLatencyMs));
}

}  // namespace

class RemoteSuggestionsFetcherImplTest : public testing::Test {
 public:
  RemoteSuggestionsFetcherImplTest()
      : default_variation_params_(
            {{"send_top_languages", "true"},
             {"send_user_class", "true"},
             {"append_request_priority_as_query_parameter", "true"}}) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ntp_snippets::kArticleSuggestionsFeature, default_variation_params_);
    UserClassifier::RegisterProfilePrefs(utils_.pref_service()->registry());
    user_classifier_ = std::make_unique<UserClassifier>(
        utils_.pref_service(), base::DefaultClock::GetInstance());
    ResetFetcher();
  }

  ~RemoteSuggestionsFetcherImplTest() override {}

  void ResetFetcher() { ResetFetcherWithAPIKey(kAPIKey); }

  void ResetFetcherWithAPIKey(const std::string& api_key) {
    scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    fetcher_ = std::make_unique<RemoteSuggestionsFetcherImpl>(
        identity_test_env_.identity_manager(),
        std::move(test_shared_loader_factory), utils_.pref_service(), nullptr,
        base::BindRepeating(&ParseJsonDelayed), GetFetchEndpoint(), api_key,
        user_classifier_.get());

    fetcher_->SetClockForTesting(task_environment_.GetMockClock());
  }

  void SignIn() { identity_test_env_.MakePrimaryAccountAvailable(kTestEmail); }

  RemoteSuggestionsFetcher::SnippetsAvailableCallback
  ToSnippetsAvailableCallback(MockSnippetsAvailableCallback* callback) {
    return base::BindOnce(&MockSnippetsAvailableCallback::WrappedRun,
                          base::Unretained(callback));
  }

  RemoteSuggestionsFetcherImpl& fetcher() { return *fetcher_; }
  MockSnippetsAvailableCallback& mock_callback() { return mock_callback_; }
  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  RequestParams test_params() {
    RequestParams result;
    result.count_to_fetch = 1;
    result.interactive_request = true;
    return result;
  }

  void SetVariationParam(std::string param_name, std::string value) {
    std::map<std::string, std::string> params = default_variation_params_;
    params[param_name] = value;

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ntp_snippets::kArticleSuggestionsFeature, params);
  }

  void SetFakeResponse(const GURL& request_url,
                       const std::string& response_data,
                       net::HttpStatusCode response_code,
                       net::Error error) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
        static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, std::move(head),
                                         response_data, status);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::map<std::string, std::string> default_variation_params_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  test::RemoteSuggestionsTestUtils utils_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<RemoteSuggestionsFetcherImpl> fetcher_;
  std::unique_ptr<UserClassifier> user_classifier_;
  MockSnippetsAvailableCallback mock_callback_;
  GURL test_url_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsFetcherImplTest);
};

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldNotFetchOnCreation) {
  // The lack of registered baked in responses would cause any fetch to fail.
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              IsEmpty());
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), IsEmpty());
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldFetchSuccessfully) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/AllOf(
                      IsSingleArticle("http://localhost/foobar"),
                      FirstCategoryHasInfo(IsCategoryInfoForArticles()))));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldExposeRequestPriorityInUrl) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=background_prefetch"),
                  /*response_data=*/"{\"categories\" : []}", net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(mock_callback(), Run(Property(&Status::IsSuccess, true),
                                   /*fetched_categories=*/_));

  RequestParams params = test_params();
  params.interactive_request = false;
  fetcher().FetchSnippets(params,
                          ToSnippetsAvailableCallback(&mock_callback()));

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldNotExposeRequestPriorityInUrlWhenDisabled) {
  SetVariationParam("append_request_priority_as_query_parameter", "false");

  SetFakeResponse(
      GURL(std::string(kFetchSuggestionsEndpoint) + "?key=fakeAPIkey"),
      /*response_data=*/"{\"categories\" : []}", net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(), Run(Property(&Status::IsSuccess, true),
                                   /*fetched_categories=*/_));

  RequestParams params = test_params();
  params.interactive_request = false;
  fetcher().FetchSnippets(params,
                          ToSnippetsAvailableCallback(&mock_callback()));

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldFetchSuccessfullyWhenSignedIn) {
  SignIn();

  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(
      GURL(std::string(kFetchSuggestionsEndpoint) + "?priority=user_action"),
      /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/AllOf(
                      IsSingleArticle("http://localhost/foobar"),
                      FirstCategoryHasInfo(IsCategoryInfoForArticles()))));

  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldExposeRequestPriorityInUrlWhenSignedIn) {
  SignIn();

  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?priority=background_prefetch"),
                  /*response_data=*/"{\"categories\" : []}", net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(mock_callback(), Run(Property(&Status::IsSuccess, true),
                                   /*fetched_categories=*/_));

  RequestParams params = test_params();
  params.interactive_request = false;
  fetcher().FetchSnippets(params,
                          ToSnippetsAvailableCallback(&mock_callback()));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldNotExposeRequestPriorityInUrlWhenDisabledWhenSignedIn) {
  SetVariationParam("append_request_priority_as_query_parameter", "false");

  SignIn();

  SetFakeResponse(GURL(kFetchSuggestionsEndpoint),
                  /*response_data=*/"{\"categories\" : []}", net::HTTP_OK,
                  net::OK);
  EXPECT_CALL(mock_callback(), Run(Property(&Status::IsSuccess, true),
                                   /*fetched_categories=*/_));

  RequestParams params = test_params();
  params.interactive_request = false;
  fetcher().FetchSnippets(params,
                          ToSnippetsAvailableCallback(&mock_callback()));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldRetryWhenOAuthCancelledWhenSignedIn) {
  SignIn();

  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(
      GURL(std::string(kFetchSuggestionsEndpoint) + "?priority=user_action"),
      /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/AllOf(
                      IsSingleArticle("http://localhost/foobar"),
                      FirstCategoryHasInfo(IsCategoryInfoForArticles()))));

  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));

  // Cancel the first access token request that's made.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::REQUEST_CANCELED));

  // RemoteSuggestionsFetcher should retry fetching an access token if the first
  // attempt is cancelled. Respond with a valid access token on the retry.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for the fake response.
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest, EmptyCategoryIsOK) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\""
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/IsEmptyArticleList()));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ServerCategories) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}, {"
      "  \"id\": 2,"
      "  \"localizedTitle\": \"Articles for Me\","
      "  \"allowFetchingMoreResults\": true,"
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foo2\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foo2\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foo2.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories;
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true), /*fetched_categories=*/_))
      .WillOnce(MoveArgument1PointeeTo(&fetched_categories));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();

  ASSERT_TRUE(fetched_categories);
  ASSERT_THAT(fetched_categories->size(), Eq(2u));
  for (const auto& category : *fetched_categories) {
    const auto& articles = category.suggestions;
    if (category.category.IsKnownCategory(KnownCategories::ARTICLES)) {
      ASSERT_THAT(articles.size(), Eq(1u));
      EXPECT_THAT(articles[0]->url().spec(), Eq("http://localhost/foobar"));
      EXPECT_THAT(category.info, IsCategoryInfoForArticles());
    } else if (category.category == Category::FromRemoteCategory(2)) {
      ASSERT_THAT(articles.size(), Eq(1u));
      EXPECT_THAT(articles[0]->url().spec(), Eq("http://localhost/foo2"));
      EXPECT_THAT(category.info.additional_action(),
                  Eq(ContentSuggestionsAdditionalAction::FETCH));
      EXPECT_THAT(category.info.show_if_empty(), Eq(false));
    } else {
      FAIL() << "unknown category ID " << category.category.id();
    }
  }

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       SupportMissingAllowFetchingMoreResultsOption) {
  // This tests makes sure we handle the missing option although it's required
  // by the interface. It's just that the Service doesn't follow that
  // requirement (yet). TODO(tschumann): remove this test once not needed
  // anymore.
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 2,"
      "  \"localizedTitle\": \"Articles for Me\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foo2\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foo2\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foo2.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories;
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true), /*fetched_categories=*/_))
      .WillOnce(MoveArgument1PointeeTo(&fetched_categories));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();

  ASSERT_TRUE(fetched_categories);
  ASSERT_THAT(fetched_categories->size(), Eq(1u));
  EXPECT_THAT(fetched_categories->front().info.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::NONE));
  EXPECT_THAT(fetched_categories->front().info.title(),
              Eq(base::UTF8ToUTF16("Articles for Me")));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ExclusiveCategoryOnly) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}, {"
      "  \"id\": 2,"
      "  \"localizedTitle\": \"Articles for Me\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foo2\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foo2\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foo2.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}, {"
      "  \"id\": 3,"
      "  \"localizedTitle\": \"Articles for Anybody\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foo3\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foo3\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foo3.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories;
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true), /*fetched_categories=*/_))
      .WillOnce(MoveArgument1PointeeTo(&fetched_categories));

  RequestParams params = test_params();
  params.exclusive_category =
      base::Optional<Category>(Category::FromRemoteCategory(2));

  fetcher().FetchSnippets(params,
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();

  ASSERT_TRUE(fetched_categories);
  ASSERT_THAT(fetched_categories->size(), Eq(1u));
  const auto& category = (*fetched_categories)[0];
  EXPECT_THAT(category.category.id(), Eq(Category::FromRemoteCategory(2).id()));
  ASSERT_THAT(category.suggestions.size(), Eq(1u));
  EXPECT_THAT(category.suggestions[0]->url().spec(),
              Eq("http://localhost/foo2"));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldNotFetchWithoutApiKey) {
  ResetFetcherWithAPIKey(std::string());

  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::PERMANENT_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              Eq("No API key available."));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              IsEmpty());
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldFetchSuccessfullyEmptyList) {
  const std::string kJsonStr = "{\"categories\": []}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/IsEmptyCategoriesList()));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(), Eq("OK"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest, RetryOnInteractiveRequests) {
  RequestParams params = test_params();
  params.interactive_request = true;

  EXPECT_THAT(
      internal::JsonRequest::Get5xxRetryCount(params.interactive_request),
      Eq(2));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       RetriesConfigurableOnNonInteractiveRequests) {
  struct ExpectationForVariationParam {
    std::string param_value;
    int expected_value;
    std::string description;
  };
  const std::vector<ExpectationForVariationParam> retry_config_expectation = {
      {"", 0, "Do not retry by default"},
      {"0", 0, "Do not retry on param value 0"},
      {"-1", 0, "Do not retry on negative param values."},
      {"4", 4, "Retry as set in param value."}};

  RequestParams params = test_params();
  params.interactive_request = false;

  for (const auto& retry_config : retry_config_expectation) {
    // DelegateCallingTestURLFetcherFactory fetcher_factory;
    SetVariationParam("background_5xx_retries_count", retry_config.param_value);

    EXPECT_THAT(internal::JsonRequest::Get5xxRetryCount(false),
                Eq(retry_config.expected_value))
        << retry_config.description;
  }
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       HttpUnauthorizedIsTreatedAsDistinctTemporaryError) {
  SignIn();
  SetFakeResponse(
      GURL(std::string(kFetchSuggestionsEndpoint) + "?priority=user_action"),
      /*response_data=*/std::string(), net::HTTP_UNAUTHORIZED, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              Eq("Access token invalid 401"));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldReportUrlStatusError) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::ERR_FAILED);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              Eq("URLRequestStatus error -2"));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/-2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldReportHttpError) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/404, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldReportJsonError) {
  const std::string kInvalidJsonStr = "{ \"recos\": []";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kInvalidJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              StartsWith("Received invalid JSON (error "));
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kInvalidJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportJsonErrorForEmptyResponse) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/std::string(), net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), std::string());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldReportInvalidListError) {
  const std::string kJsonStr =
      "{\"recos\": [{ \"contentInfo\": { \"foo\" : \"bar\" }}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastJsonForDebugging(), Eq(kJsonStr));
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              StartsWith("Invalid / empty list"));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportInvalidListErrorForIncompleteSuggestionButValidJson) {
  // This is valid json, but it does not represent a valid suggestion
  // (fullPageUrl is missing).
  const std::string kValidJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"INVALID_fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kValidJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              StartsWith("Invalid / empty list"));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportInvalidListErrorForInvalidTimestampButValidJson) {
  // This is valid json, but it does not represent a valid suggestion
  // (creationTime is invalid).
  const std::string kValidJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"INVALID_2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kValidJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              StartsWith("Invalid / empty list"));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportInvalidListErrorForInvalidUrlButValidJson) {
  // This is valid json, but it does not represent a valid suggestion
  // (URL is invalid).
  const std::string kValidJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"NOT A URL\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"NOT A URL\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kValidJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(fetcher().GetLastStatusForDebugging(),
              StartsWith("Invalid / empty list"));
}

TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportRequestFailureAsTemporaryError) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::ERR_FAILED);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
}

// This test actually verifies that the test setup itself is sane, to prevent
// hard-to-reproduce test failures.
TEST_F(RemoteSuggestionsFetcherImplTest,
       ShouldReportHttpErrorForMissingBakedResponse) {
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::ERR_FAILED);
  EXPECT_CALL(mock_callback(),
              Run(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                  /*fetched_categories=*/IsNullCategoriesList()))
      .Times(1);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
}

TEST_F(RemoteSuggestionsFetcherImplTest, ShouldProcessConcurrentFetches) {
  const std::string kJsonStr = "{ \"categories\": [] }";
  SetFakeResponse(GURL(std::string(kFetchSuggestionsEndpoint) +
                       "?key=fakeAPIkey&priority=user_action"),
                  /*response_data=*/kJsonStr, net::HTTP_OK, net::OK);
  EXPECT_CALL(mock_callback(),
              Run(Property(&Status::IsSuccess, true),
                  /*fetched_categories=*/IsEmptyCategoriesList()))
      .Times(5);
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  // More calls to FetchSnippets() do not interrupt the previous.
  // Callback is expected to be called once each time.
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  fetcher().FetchSnippets(test_params(),
                          ToSnippetsAvailableCallback(&mock_callback()));
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/5)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/5)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/5)));
}

::std::ostream& operator<<(
    ::std::ostream& os,
    const RemoteSuggestionsFetcher::OptionalFetchedCategories&
        fetched_categories) {
  if (fetched_categories) {
    // Matchers above aren't any more precise than this, so this is sufficient
    // for test-failure diagnostics.
    return os << "list with " << fetched_categories->size() << " elements";
  }
  return os << "null";
}

}  // namespace ntp_snippets
