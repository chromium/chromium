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
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/page_vertical.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace action_chips {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
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
      "google:suggesttype": ["QUERY", "QUERY"]
    }
  ])json";

constexpr char kSampleSuggestionsResponseWithEmptySuggestion[] = R"json([
    "",
    ["suggestion 1", "suggestion 2", ""],
    ["", ""],
    [],
    {
      "google:suggestsubtypes": [
        [437],
        [437],
        [437]
      ],
      "google:suggesttype": ["QUERY", "QUERY", "QUERY"],
      "google:suggestdetail": [
        {},
        {},
        {
          "google:suggesttemplate": "CAIaGAoWQXNrIGFib3V0IHByZXZpb3VzIHRhYiIaChgiU29sdmUgbGluZWFyIGVxdWF0aW9ucyI=",
        }
      ],
    }
  ])json";

// The following is an example of an empty response.
constexpr char kEmptySuggestionsResponse[] =
    R"json([ "", [], [], [], {} ])json";

struct GetSuggestURLOptions {
  std::optional<std::string> page_url;
  std::optional<std::string> title;
  std::vector<omnibox::ToolMode> allowed_tools;
  std::optional<omnibox::PageVertical> page_vertical;
};

GURL GetSuggestURL(const GetSuggestURLOptions& options,
                   AutocompleteProviderClient& ac_client) {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  if (options.page_url.has_value()) {
    search_terms_args.current_page_url = *options.page_url;
  }

  std::vector<std::string> additional_query_params;

  if (options.allowed_tools.empty()) {
    search_terms_args.focus_type = metrics::OmniboxFocusType::INTERACTION_FOCUS;
    search_terms_args.page_classification = metrics::OmniboxEventProto::OTHER;
    additional_query_params.push_back("ctxus=1");
    if (options.title.has_value()) {
      additional_query_params.push_back(base::StrCat(
          {"pageTitle=", url::UriComponentEncoder(*options.title).view()}));
    }
  } else {
    search_terms_args.page_classification =
        metrics::OmniboxEventProto::NTP_ZPS_PREFETCH;
    search_terms_args.request_source =
        SearchTermsData::RequestSource::NTP_ACTION_CHIPS;

    std::vector<std::string> allowed_tools_strings;
    allowed_tools_strings.reserve(options.allowed_tools.size());
    for (const auto& tool : options.allowed_tools) {
      allowed_tools_strings.push_back(base::NumberToString(tool));
    }
    additional_query_params.push_back(
        base::StrCat({"ats=", base::JoinString(allowed_tools_strings, ",")}));

    if (options.title.has_value()) {
      additional_query_params.push_back(base::StrCat(
          {"pageTitle=", url::UriComponentEncoder(*options.title).view()}));
    }
    if (options.page_vertical.has_value()) {
      additional_query_params.push_back(base::StrCat(
          {"pageVertical=", base::NumberToString(*options.page_vertical)}));
    }
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
    std::string_view expected_url_spec) {
  return base::StrCat({"Pending requests: ", GetPendingRequestUrls(factory),
                       "\nExpected: ", expected_url_spec});
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
  void GetDeepdiveChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) {
    loader_ = service_.GetDeepdiveChipSuggestionsForTab(title, url,
                                                        std::move(callback));
  }

  void GetActionChipSuggestions(
      base::optional_ref<const std::u16string> title,
      base::optional_ref<const GURL> url,
      base::span<const omnibox::ToolMode> allowed_tools,
      base::optional_ref<const omnibox::PageVertical> page_vertical,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) {
    loader_ = service_.GetActionChipSuggestions(
        title, url, allowed_tools, page_vertical, std::move(callback));
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

using GetDeepdiveChipSuggestionsForTabHappyPathTest =
    testing::TestWithParam<HappyPathTestCase>;

TEST_P(GetDeepdiveChipSuggestionsForTabHappyPathTest,
       ReturnsParsedResultWhenValidResponseIsReturned) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title with spaces";
  const GURL current_url("https://example.com/");
  context.GetDeepdiveChipSuggestionsForTab(
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
    GetDeepdiveChipSuggestionsForTabHappyPathTest,
    testing::Values(
        HappyPathTestCase{
            kSampleSuggestionsResponse,
            {ExpectedSuggestion{u"suggestion 1",
                                AutocompleteMatchType::SEARCH_SUGGEST},
             ExpectedSuggestion{u"suggestion 2",
                                AutocompleteMatchType::SEARCH_SUGGEST}}},
        HappyPathTestCase{kEmptySuggestionsResponse, {}}));

using GetActionChipSuggestionsHappyPathTest =
    testing::TestWithParam<HappyPathTestCase>;

TEST_P(GetActionChipSuggestionsHappyPathTest,
       ReturnsParsedResultWhenValidResponseIsReturned) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title with spaces";
  const GURL current_url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};

  context.GetActionChipSuggestions(
      title, current_url, allowed_tools, /*page_vertical=*/std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));
  const GURL suggest_url = GetSuggestURL({.page_url = current_url.spec(),
                                          .title = base::UTF16ToUTF8(title),
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
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
    GetActionChipSuggestionsHappyPathTest,
    testing::Values(
        HappyPathTestCase{
            kSampleSuggestionsResponseWithEmptySuggestion,
            {ExpectedSuggestion{u"suggestion 1",
                                AutocompleteMatchType::SEARCH_SUGGEST},
             ExpectedSuggestion{u"suggestion 2",
                                AutocompleteMatchType::SEARCH_SUGGEST},
             ExpectedSuggestion{u"", AutocompleteMatchType::SEARCH_SUGGEST}}},
        HappyPathTestCase{kEmptySuggestionsResponse, {}}));

TEST(RemoteSuggestionsServiceSimpleTest,
     GetDeepdiveChipSuggestionsForTabReturnsTimeoutError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetDeepdiveChipSuggestionsForTab(
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetDeepdiveChipSuggestionsForTabFailsOnNetworkError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetDeepdiveChipSuggestionsForTab(
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetDeepdiveChipSuggestionsForTabFailsOnHttpError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetDeepdiveChipSuggestionsForTab(
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetDeepdiveChipSuggestionsForTabFailsOnMalformedJson) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetDeepdiveChipSuggestionsForTab(
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

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(
                          FieldsAre(RemoteSuggestionsServiceSimple::
                                        ParseFailureReason::kMalformedJson))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetDeepdiveChipSuggestionsForTabFailsOnParseFailure) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  context.GetDeepdiveChipSuggestionsForTab(
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

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(
                          FieldsAre(RemoteSuggestionsServiceSimple::
                                        ParseFailureReason::kSchemaMismatch))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsReturnsParsedResult) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"page title";
  const GURL current_url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
      omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH};
  const omnibox::PageVertical page_vertical =
      omnibox::PageVertical::PAGE_VERTICAL_EDU;

  context.GetActionChipSuggestions(
      title, current_url, allowed_tools, page_vertical,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = current_url.spec(),
                                          .title = base::UTF16ToUTF8(title),
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = page_vertical},
                                         context.client());

  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "ats", &value));
  EXPECT_EQ(value,
            base::StrCat({base::NumberToString(static_cast<int>(
                              omnibox::ToolMode::TOOL_MODE_IMAGE_GEN)),
                          ",",
                          base::NumberToString(static_cast<int>(
                              omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH))}));
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "pageVertical", &value));
  EXPECT_EQ(value, base::NumberToString(static_cast<int>(page_vertical)));
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "client", &value));
  EXPECT_EQ(value, "chrome-ntp-action");

  context.client().test_url_loader_factory()->AddResponse(
      suggest_url.spec(), kSampleSuggestionsResponse);
  env.run_loop().Run();

  EXPECT_THAT(actual,
              ValueIs(ElementsAre(
                  Property(&SearchSuggestionParser::SuggestResult::suggestion,
                           u"suggestion 1"),
                  Property(&SearchSuggestionParser::SuggestResult::suggestion,
                           u"suggestion 2"))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsWithoutPageVertical) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"page title";
  const GURL current_url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};

  context.GetActionChipSuggestions(
      title, current_url, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = current_url.spec(),
                                          .title = base::UTF16ToUTF8(title),
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());

  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "ats", &value));
  EXPECT_EQ(value, "4");
  EXPECT_FALSE(net::GetValueForKeyInQuery(suggest_url, "pageVertical", &value));
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "client", &value));
  EXPECT_EQ(value, "chrome-ntp-action");

  context.client().test_url_loader_factory()->AddResponse(
      suggest_url.spec(), kSampleSuggestionsResponse);
  env.run_loop().Run();

  EXPECT_TRUE(actual.has_value());
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsReturnsTimeoutError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  context.GetActionChipSuggestions(
      title, url, allowed_tools, std::nullopt,
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsFailsOnNetworkError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  context.GetActionChipSuggestions(
      title, url, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = url.spec(),
                                          .title = "title",
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsFailsOnHttpError) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  context.GetActionChipSuggestions(
      title, url, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = url.spec(),
                                          .title = "title",
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsFailsOnMalformedJson) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  context.GetActionChipSuggestions(
      title, url, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = url.spec(),
                                          .title = "title",
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                          "invalid json");
  env.run_loop().Run();

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(
                          FieldsAre(RemoteSuggestionsServiceSimple::
                                        ParseFailureReason::kMalformedJson))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsFailsOnParseFailure) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  context.GetActionChipSuggestions(
      title, url, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = url.spec(),
                                          .title = "title",
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());
  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());
  context.client().test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                          "[]");
  env.run_loop().Run();

  EXPECT_THAT(actual, ErrorIs(VariantWith<ParseError>(
                          FieldsAre(RemoteSuggestionsServiceSimple::
                                        ParseFailureReason::kSchemaMismatch))));
}

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsWithEmptyTitleAndUrl) {
  EnvironmentFixture env;
  ServiceTestContext context;

  RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult actual;
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};

  context.GetActionChipSuggestions(
      std::nullopt, std::nullopt, allowed_tools, std::nullopt,
      base::BindLambdaForTesting(
          [&actual,
           &env](RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&
                     result) {
            actual = std::move(result);
            env.run_loop().Quit();
          }));

  const GURL suggest_url = GetSuggestURL({.page_url = std::nullopt,
                                          .title = std::nullopt,
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         context.client());

  ASSERT_TRUE(
      context.client().test_url_loader_factory()->IsPending(suggest_url.spec()))
      << GeneratePendingRequestsDebugMsg(
             *context.client().test_url_loader_factory(), suggest_url.spec());

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "ats", &value));
  EXPECT_EQ(value, base::NumberToString(static_cast<int>(
                       omnibox::ToolMode::TOOL_MODE_IMAGE_GEN)));
  EXPECT_FALSE(net::GetValueForKeyInQuery(suggest_url, "pageTitle", &value));
  EXPECT_FALSE(net::GetValueForKeyInQuery(suggest_url, "url", &value));
  EXPECT_TRUE(net::GetValueForKeyInQuery(suggest_url, "client", &value));
  EXPECT_EQ(value, "chrome-ntp-action");

  context.client().test_url_loader_factory()->AddResponse(
      suggest_url.spec(), kSampleSuggestionsResponse);
  env.run_loop().Run();

  EXPECT_TRUE(actual.has_value());
}

TEST(
    RemoteSuggestionsServiceSimpleTest,
    GetDeepdiveChipSuggestionsForTabDoesNotCrashOnServiceDestructionDuringFetch) {
  EnvironmentFixture env;
  FakeAutocompleteProviderClient client;
  base::MockCallback<base::OnceCallback<void(
      RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>>
      callback;

  auto service = std::make_unique<RemoteSuggestionsServiceSimpleImpl>(&client);

  EXPECT_CALL(callback, Run(_)).Times(0);

  std::unique_ptr<network::SimpleURLLoader> loader =
      service->GetDeepdiveChipSuggestionsForTab(
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

TEST(RemoteSuggestionsServiceSimpleTest,
     GetActionChipSuggestionsDoesNotCrashOnServiceDestructionDuringFetch) {
  EnvironmentFixture env;
  FakeAutocompleteProviderClient client;
  base::MockCallback<base::OnceCallback<void(
      RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>>
      callback;

  auto service = std::make_unique<RemoteSuggestionsServiceSimpleImpl>(&client);

  EXPECT_CALL(callback, Run(_)).Times(0);

  const std::u16string title = u"title";
  const GURL url("https://example.com/");
  const std::vector<omnibox::ToolMode> allowed_tools = {
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN};
  std::unique_ptr<network::SimpleURLLoader> loader =
      service->GetActionChipSuggestions(title, url, allowed_tools, std::nullopt,
                                        callback.Get());

  // Destroy the service.
  service.reset();

  // Fast forward time to ensure any pending tasks would have run.
  env.FastForwardBy(base::Seconds(100));

  // Also complete the network request to ensure it doesn't trigger callback on
  // dead object.
  const GURL suggest_url = GetSuggestURL({.page_url = url.spec(),
                                          .title = "title",
                                          .allowed_tools = allowed_tools,
                                          .page_vertical = std::nullopt},
                                         client);
  if (client.test_url_loader_factory()->IsPending(suggest_url.spec())) {
    client.test_url_loader_factory()->AddResponse(suggest_url.spec(),
                                                  kSampleSuggestionsResponse);
  }

  // Verify callback was not called.
}
}  // namespace
}  // namespace action_chips
