// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_test_utils.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace action_chips {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::VariantWith;

using NetworkError = RemoteSuggestionsServiceSimple::NetworkError;
using ParseError = RemoteSuggestionsServiceSimple::ParseError;

// The real response from the remote endpoint is a list of suggestions in JSON
// format.
constexpr char kSampleSuggestionsResponse[] = R"json([
    "",
    ["suggestion 1", "suggestion 2"],
    ["", ""],
    [],
    {
      "google:suggestsubtypes": [
        [437],
        [437]
      ],
      "google:suggesttype": "QUERY"
    }
  ])json";

// The following is an example of an empty response.
constexpr char kEmptySuggestionsResponse[] =
    R"json([ "", [], [], [], {} ])json";

struct GetSuggestURLOptions {
  std::string page_url;
  std::string title;
};

GURL GetSuggestURL(const GetSuggestURLOptions& options,
                   AutocompleteProviderClient& ac_client) {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.page_classification = metrics::OmniboxEventProto::OTHER;
  search_terms_args.focus_type = metrics::OmniboxFocusType::INTERACTION_FOCUS;

  search_terms_args.current_page_url = options.page_url;

  std::vector<std::string> additional_query_params;
  if (!options.title.empty()) {
    url::RawCanonOutputT<char> encoded_title;
    url::EncodeURIComponent(options.title, &encoded_title);
    additional_query_params.push_back("ctxus=1");
    additional_query_params.push_back(
        base::StrCat({"pageTitle=", encoded_title.view()}));
  }

  search_terms_args.additional_query_params =
      base::JoinString(additional_query_params, "&");

  TemplateURLService* template_url_service = ac_client.GetTemplateURLService();
  return RemoteSuggestionsService::EndpointUrl(
      *template_url_service->GetDefaultSearchProvider(), search_terms_args,
      template_url_service->search_terms_data());
}

std::string GetPendingRequestUrls(network::TestURLLoaderFactory& factory) {
  const std::vector<network::TestURLLoaderFactory::PendingRequest>&
      pending_requests = *factory.pending_requests();
  if (pending_requests.empty()) {
    return "None";
  } else {
    std::vector<std::string> urls;
    std::ranges::transform(
        pending_requests, std::back_inserter(urls),
        [](const network::TestURLLoaderFactory::PendingRequest& request) {
          return request.request.url.spec();
        });
    return base::JoinString(urls, "\n");
  }
}

std::string GeneratePendingRequestsDebugMsg(
    network::TestURLLoaderFactory& factory,
    const std::string& expected_url_spec) {
  return "Pending requests: " + GetPendingRequestUrls(factory) +
         "\nExpected: " + expected_url_spec;
}

// A fixture to initialize and operate on the execution environment.
class EnvironmentFixture {
 public:
  void FastForwardBy(const base::TimeDelta& delta) {
    task_environment_.FastForwardBy(delta);
  }
  base::RunLoop& run_loop() { return run_loop_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::RunLoop run_loop_;
};

// A fixture for the unit under test.
class ServiceTestContext {
 public:
  FakeAutocompleteProviderClient& client() { return client_; }
  void GetActionChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) {
    loader_ = service_.GetActionChipSuggestionsForTab(title, url,
                                                      std::move(callback));
  }

  void CancelRequest() { loader_.reset(); }

 private:
  FakeAutocompleteProviderClient client_;
  RemoteSuggestionsServiceSimpleImpl service_{&client_};
  // This field is to ease the lifetime management of the loader.
  // With this, test authors do not have to remember that we need to retain the
  // returned loader.
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

struct ExpectedSuggestion {
  std::u16string suggestion;
  AutocompleteMatchType::Type type;
};

struct HappyPathTestCase {
  const std::string response;
  const std::vector<ExpectedSuggestion> expected_suggestions;
};

using RemoteSuggestionsServiceSimpleHappyPathTest =
    testing::TestWithParam<HappyPathTestCase>;

TEST_P(RemoteSuggestionsServiceSimpleHappyPathTest,
       ReturnsParsedResultWhenValidResponseIsReturned) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title with spaces";
  const GURL current_url("https://example.com/");
  context.GetActionChipSuggestionsForTab(
      title, current_url,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));
  const GURL suggest_url = GetSuggestURL(
      {.page_url = current_url.spec(), .title = base::UTF16ToUTF8(title)},
      context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                          GetParam().response);
  env.run_loop().Run();

  std::vector<Matcher<const SearchSuggestionParser::SuggestResult>> matchers;
  for (const ExpectedSuggestion& s : GetParam().expected_suggestions) {
    matchers.push_back(
        AllOf(Property(&SearchSuggestionParser::SuggestResult::suggestion,
                       s.suggestion),
              Property(&SearchSuggestionParser::SuggestResult::type, s.type)));
  }
  EXPECT_THAT(actual, ValueIs(ElementsAreArray(matchers)));
}

INSTANTIATE_TEST_SUITE_P(
    RemoteSuggestionsServiceSimpleTests,
    RemoteSuggestionsServiceSimpleHappyPathTest,
    testing::Values(
        HappyPathTestCase{
            kSampleSuggestionsResponse,
            {ExpectedSuggestion{u"suggestion 1",
                                AutocompleteMatchType::SEARCH_SUGGEST},
             ExpectedSuggestion{u"suggestion 2",
                                AutocompleteMatchType::SEARCH_SUGGEST}}},
        HappyPathTestCase{kEmptySuggestionsResponse, {}}));

TEST(RemoteSuggestionsServiceSimpleTest, ReturnsTimeoutErrorWhenTimeouts) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetActionChipSuggestionsForTab(
      u"title", GURL("https://example.com/"),
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  env.FastForwardBy(base::Seconds(100));
  env.run_loop().Run();

  EXPECT_THAT(actual, ErrorIs(VariantWith<NetworkError>(
                          FieldsAre(net::Error::ERR_TIMED_OUT, 0))));
}

TEST(RemoteSuggestionsServiceSimpleTest, FailsOnNetworkError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetActionChipSuggestionsForTab(
      u"title", GURL("https://example.com/"),
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL(
      {.page_url = "https://example.com/", .title = "title"}, context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(
      suggest_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::Error::ERR_FAILED));
  env.run_loop().Run();

  EXPECT_THAT(
      actual,
      ErrorIs(VariantWith<NetworkError>(FieldsAre(net::Error::ERR_FAILED, 0))));
}

TEST(RemoteSuggestionsServiceSimpleTest, FailsOnHttpError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetActionChipSuggestionsForTab(
      u"title", GURL("https://example.com/"),
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL(
      {.page_url = "https://example.com/", .title = "title"}, context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(
      suggest_url.spec(), "", net::HTTP_NOT_FOUND);
  env.run_loop().Run();

  EXPECT_THAT(actual,
              ErrorIs(VariantWith<NetworkError>(FieldsAre(
                  net::ERR_HTTP_RESPONSE_CODE_FAILURE, net::HTTP_NOT_FOUND))));
}

TEST(RemoteSuggestionsServiceSimpleTest, FailsOnMalformedJson) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetActionChipSuggestionsForTab(
      u"title", GURL("https://example.com/"),
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL(
      {.page_url = "https://example.com/", .title = "title"}, context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                          "invalid json");
  env.run_loop().Run();

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(FieldsAre(
                          ParseError::ParseErrorType::kMalformedJson))));
}

TEST(RemoteSuggestionsServiceSimpleTest, FailsOnParseFailure) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetActionChipSuggestionsForTab(
      u"title", GURL("https://example.com/"),
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL(
      {.page_url = "https://example.com/", .title = "title"}, context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                          "[]");
  env.run_loop().Run();

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(FieldsAre(
                          ParseError::ParseErrorType::kParseFailure))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     DoesNotCrashOnServiceDestructionDuringFetch) {
  EnvironmentFixture env;
  FakeAutocompleteProviderClient client;
  base::MockCallback<base::OnceCallback<void(
      RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>>
      callback;

  auto service = std::make_unique<RemoteSuggestionsServiceSimpleImpl>(&client);

  EXPECT_CALL(callback, Run(_)).Times(0);

  std::unique_ptr<network::SimpleURLLoader> loader =
      service->GetActionChipSuggestionsForTab(
          u"title", GURL("https://example.com/"), callback.Get());

  // Destroy the service.
  service.reset();

  // Fast forward time to ensure any pending tasks would have run.
  env.FastForwardBy(base::Seconds(100));

  // Also complete the network request to ensure it doesn't trigger callback on
  // dead object.
  const GURL suggest_url = GetSuggestURL(
      {.page_url = "https://example.com/", .title = "title"}, client);
  if (client.test_url_loader_factory()->IsPending(suggest_url.spec())) {
    client.test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                  kSampleSuggestionsResponse);
  }

  // Verify callback was not called.
}
}  // namespace
}  // namespace action_chips
