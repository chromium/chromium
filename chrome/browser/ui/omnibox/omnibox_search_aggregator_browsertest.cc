// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/enterprise/search_aggregator_policy_handler.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using testing::AllOf;
using testing::Field;

namespace {

using ui_test_utils::WaitForAutocompleteDone;

const char kSearchAggregatorPolicyIconUrl[] =
    "https://www.aggregator.com/icon.png";
const char16_t kSearchAggregatorPolicyKeyword[] = u"aggregator";
const char16_t kSearchAggregatorPolicyName[] = u"Aggregator";
const char kSearchAggregatorPolicySearchUrl[] =
    "https://www.aggregator.com/search?q={searchTerms}";
const char kSearchAggregatorPolicySuggestUrl[] =
    "https://www.aggregator.com/suggest";

const char kSearchAggregatorPolicySuggestPath[] = "/suggest";
const std::u16string kSearchInput = u"@aggregator john d";

const std::string kGoodJsonResponse = base::StringPrintf(
    R"({
        "querySuggestions": [
          {
            "suggestion": "John's Demise",
            "score": 0.1,
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
            "destinationUri": "https://www.example.com/people/john",
            "score": 0.8,
            "dataStore": "project 1"
          }
        ],
        "contentSuggestions": [
          {
            "suggestion": "John's Document",
            "contentType": "THIRD_PARTY",
            "document": {
              "name": "Document 2",
              "structData": {
                "title": "John's Document",
                "uri": "www.example.com"
              },
              "derivedStructData": {
                "source_type": "jira",
                "entity_type": "issue",
                "title": "John's Document",
                "link": "https://www.example.co.uk"
              }
            },
            "destinationUri": "https://www.example.com/",
            "score": 0.4,
            "dataStore": "project2"
          }
        ]
      })");

}  // namespace

class OmniboxSearchAggregatorTest : public InProcessBrowserTest {
 public:
  OmniboxSearchAggregatorTest() = default;
  ~OmniboxSearchAggregatorTest() override = default;

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // The omnibox suggestion results depend on the TemplateURLService being
    // loaded. Make sure it is loaded so that the autocomplete results are
    // consistent.
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));

    // Prevent the stop timer from killing the hints fetch early, which might
    // cause test flakiness due to timeout.
    controller()->SetStartStopTimerDurationForTesting(base::Seconds(30));

    // Setup an identity profile.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env()->SetPrimaryAccount("test@mail.com",
                                           signin::ConsentLevel::kSignin);
    identity_test_env()->SetRefreshTokenForPrimaryAccount();
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

    // Initial the HTTPS embedded test server.
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->RegisterRequestHandler(base::BindRepeating(
        &OmniboxSearchAggregatorTest::HandleRequest, base::Unretained(this)));
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&OmniboxSearchAggregatorTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // The kSuggestUrl will be hit by GET request from SearchProvider, we could
    // ignore it for SearchAggregator tests.
    if (request.relative_url == kSearchAggregatorPolicySuggestPath &&
        request.method == net::test_server::METHOD_GET) {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_NOT_FOUND);
      return std::move(http_response);
    }
    return nullptr;
  }

  base::Value CreateEnterpriseSearchAggregatorPolicyValue(
      const std::string& suggest_url = kSearchAggregatorPolicySuggestUrl) {
    base::Value::Dict policy_value;
    policy_value = base::Value::Dict()
                       .Set(policy::SearchAggregatorPolicyHandler::kIconUrl,
                            kSearchAggregatorPolicyIconUrl)
                       .Set(policy::SearchAggregatorPolicyHandler::kShortcut,
                            kSearchAggregatorPolicyKeyword)
                       .Set(policy::SearchAggregatorPolicyHandler::kName,
                            kSearchAggregatorPolicyName)
                       .Set(policy::SearchAggregatorPolicyHandler::kSearchUrl,
                            kSearchAggregatorPolicySearchUrl)
                       .Set(policy::SearchAggregatorPolicyHandler::kSuggestUrl,
                            suggest_url);
    return base::Value(std::move(policy_value));
  }

  AutocompleteController* controller() {
    return browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxView()
        ->controller()
        ->autocomplete_controller();
  }

  // Override embedded_test_server() with a variant that uses HTTPS.
  net::EmbeddedTestServer* embedded_test_server() {
    return https_test_server_.get();
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // SearchAggregatorPolicy requires HTTPS scheme.
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

IN_PROC_BROWSER_TEST_F(OmniboxSearchAggregatorTest, GoodJsonResponse) {
  scoped_config_.Get().multiple_requests = false;
  net::test_server::ControllableHttpResponse search_aggregator_response(
      embedded_test_server(), kSearchAggregatorPolicySuggestPath);
  ASSERT_TRUE(embedded_test_server()->Start());

  base::Value policy_value = CreateEnterpriseSearchAggregatorPolicyValue(
      embedded_test_server()
          ->GetURL(kSearchAggregatorPolicySuggestPath)
          .spec());
  policy::PolicyMap policies;
  policies.Set(policy::key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::move(policy_value), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  AutocompleteInput input(
      kSearchInput, metrics::OmniboxEventProto::NTP,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  controller()->Start(input);

  // Respond for SearchAggregator request.
  search_aggregator_response.WaitForRequest();
  EXPECT_EQ(search_aggregator_response.http_request()->method,
            net::test_server::METHOD_POST);
  EXPECT_EQ(
      search_aggregator_response.http_request()->content,
      base::StringPrintf(R"({"experimentIds":["%s"],)"
                         R"("query":"john d","suggestionTypes":[1,2,3,5]})",
                         kEnterpriseSearchAggregatorExperimentId));
  search_aggregator_response.Send(net::HTTP_OK, "application/json",
                                  kGoodJsonResponse);
  search_aggregator_response.Done();

  // Wait for the autocomplete controller to finish.
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());
  const AutocompleteResult& result = controller()->result();
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 4u);

  EXPECT_THAT(
      std::vector<AutocompleteMatch>(result.begin(), result.end()),
      testing::ElementsAreArray(std::vector<
                                testing::Matcher<AutocompleteMatch>>({
          AllOf(Field(&AutocompleteMatch::type,
                      AutocompleteMatchType::SEARCH_OTHER_ENGINE),
                Field(&AutocompleteMatch::contents, u"john d"),
                Field(&AutocompleteMatch::description, u"Aggregator Search"),
                Field(&AutocompleteMatch::destination_url,
                      GURL("https://www.aggregator.com/search?q=john+d"))),
          AllOf(
              Field(&AutocompleteMatch::type,
                    AutocompleteMatchType::SEARCH_SUGGEST),
              Field(&AutocompleteMatch::contents, u"John's Demise"),
              Field(&AutocompleteMatch::description, u""),
              Field(
                  &AutocompleteMatch::destination_url,
                  GURL("https://www.aggregator.com/search?q=John%27s+Demise"))),
          AllOf(Field(&AutocompleteMatch::type,
                      AutocompleteMatchType::NAVSUGGEST),
                Field(&AutocompleteMatch::contents, u"Aggregator People"),
                Field(&AutocompleteMatch::description, u"John Doe"),
                Field(&AutocompleteMatch::destination_url,
                      GURL("https://www.example.com/people/john")),
                Field(&AutocompleteMatch::image_url,
                      GURL("https://example.com/image.png"))),
          AllOf(Field(&AutocompleteMatch::type,
                      AutocompleteMatchType::NAVSUGGEST),
                Field(&AutocompleteMatch::contents, u"Jira Issue"),
                Field(&AutocompleteMatch::description, u"John's Document"),
                Field(&AutocompleteMatch::destination_url,
                      GURL("https://www.example.com/"))),
      })));
}

IN_PROC_BROWSER_TEST_F(OmniboxSearchAggregatorTest,
                       GoodJsonResponseMultipleRequests) {
  scoped_config_.Get().multiple_requests = true;
  net::test_server::ControllableHttpResponse search_aggregator_response(
      embedded_test_server(), kSearchAggregatorPolicySuggestPath);
  ASSERT_TRUE(embedded_test_server()->Start());

  base::Value policy_value = CreateEnterpriseSearchAggregatorPolicyValue(
      embedded_test_server()
          ->GetURL(kSearchAggregatorPolicySuggestPath)
          .spec());
  policy::PolicyMap policies;
  policies.Set(policy::key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::move(policy_value), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  AutocompleteInput input(
      kSearchInput, metrics::OmniboxEventProto::NTP,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  controller()->Start(input);

  // Respond for the first SearchAggregator request.
  search_aggregator_response.WaitForRequest();
  EXPECT_EQ(search_aggregator_response.http_request()->method,
            net::test_server::METHOD_POST);
  EXPECT_EQ(search_aggregator_response.http_request()->content,
            base::StringPrintf(R"({"experimentIds":["%s"],)"
                               R"("query":"john d","suggestionTypes":[2]})",
                               kEnterpriseSearchAggregatorExperimentId));
  search_aggregator_response.Send(net::HTTP_OK, "application/json",
                                  kGoodJsonResponse);
  search_aggregator_response.Done();

  // Wait for the autocomplete controller to finish.
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());
  const AutocompleteResult& result = controller()->result();
  ASSERT_FALSE(result.empty());

  // When making multiple async requests, only check first match.
  EXPECT_THAT(
      std::vector<AutocompleteMatch>(result.begin(), result.begin() + 1),
      testing::ElementsAreArray(
          std::vector<testing::Matcher<AutocompleteMatch>>({
              AllOf(
                  Field(&AutocompleteMatch::type,
                        AutocompleteMatchType::SEARCH_OTHER_ENGINE),
                  Field(&AutocompleteMatch::contents, u"john d"),
                  Field(&AutocompleteMatch::description, u"Aggregator Search"),
                  Field(&AutocompleteMatch::destination_url,
                        GURL("https://www.aggregator.com/search?q=john+d"))),
          })));
}

IN_PROC_BROWSER_TEST_F(OmniboxSearchAggregatorTest, RedirectedResponse) {
  scoped_config_.Get().multiple_requests = false;
  net::test_server::ControllableHttpResponse redirect_response(
      embedded_test_server(), kSearchAggregatorPolicySuggestPath);
  const std::string redirected_path = "/suggest-redirect";
  net::test_server::ControllableHttpResponse search_aggregator_response(
      embedded_test_server(), redirected_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  base::Value policy_value = CreateEnterpriseSearchAggregatorPolicyValue(
      embedded_test_server()
          ->GetURL(kSearchAggregatorPolicySuggestPath)
          .spec());
  policy::PolicyMap policies;
  policies.Set(policy::key::kEnterpriseSearchAggregatorSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, std::move(policy_value), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  AutocompleteInput input(
      kSearchInput, metrics::OmniboxEventProto::NTP,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  controller()->Start(input);

  // Redirect response.
  redirect_response.WaitForRequest();
  EXPECT_EQ(redirect_response.http_request()->method,
            net::test_server::METHOD_POST);
  redirect_response.Send(net::HTTP_MOVED_PERMANENTLY, "text/html",
                         /* content = */ "", /* cookies = */ {},
                         {base::StrCat({"Location: ", redirected_path})});
  redirect_response.Done();
  // Respond for SearchAggregator request.
  search_aggregator_response.WaitForRequest();
  EXPECT_EQ(search_aggregator_response.http_request()->method,
            net::test_server::METHOD_GET);
  search_aggregator_response.Send(net::HTTP_OK, "application/json",
                                  kGoodJsonResponse);
  search_aggregator_response.Done();

  // Wait for the autocomplete controller to finish.
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());
  const AutocompleteResult& result = controller()->result();
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 4u);
}

class OmniboxSearchAggregatorHTTPErrorTest
    : public OmniboxSearchAggregatorTest,
      public ::testing::WithParamInterface<net::HttpStatusCode> {
 public:
  net::HttpStatusCode GetHttpStatusCode() { return GetParam(); }
  void SetUpOnMainThread() override {
    OmniboxSearchAggregatorTest::SetUpOnMainThread();

    // Handle search aggregator response
    search_aggregator_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kSearchAggregatorPolicySuggestPath);
    ASSERT_TRUE(embedded_test_server()->Start());

    InitSearchAggregatorPolicyConfig();
  }

 protected:
  void InitSearchAggregatorPolicyConfig() {
    base::Value policy_value = CreateEnterpriseSearchAggregatorPolicyValue(
        embedded_test_server()
            ->GetURL(kSearchAggregatorPolicySuggestPath)
            .spec());
    policy::PolicyMap policies;
    policies.Set(policy::key::kEnterpriseSearchAggregatorSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(policy_value), nullptr);
    policy_provider()->UpdateChromePolicy(policies);
  }

  net::test_server::ControllableHttpResponse* search_aggregator_response() {
    return search_aggregator_response_.get();
  }

 private:
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      search_aggregator_response_;
};

IN_PROC_BROWSER_TEST_P(OmniboxSearchAggregatorHTTPErrorTest,
                       HTTPErrorResponse) {
  scoped_config_.Get().multiple_requests = true;
  AutocompleteInput input(
      kSearchInput, metrics::OmniboxEventProto::NTP,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  controller()->Start(input);

  // Respond for SearchAggregator request.
  search_aggregator_response()->WaitForRequest();
  EXPECT_EQ(search_aggregator_response()->http_request()->method,
            net::test_server::METHOD_POST);
  EXPECT_EQ(search_aggregator_response()->http_request()->content,
            base::StringPrintf(R"({"experimentIds":["%s"],)"
                               R"("query":"john d","suggestionTypes":[2]})",
                               kEnterpriseSearchAggregatorExperimentId));
  search_aggregator_response()->Send(GetHttpStatusCode());
  search_aggregator_response()->Done();

  // Wait for the autocomplete controller to finish.
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());
  const AutocompleteResult& result = controller()->result();
  ASSERT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 1u);
  EXPECT_THAT(
      result.default_match(),
      AllOf(Field(&AutocompleteMatch::type,
                  AutocompleteMatchType::SEARCH_OTHER_ENGINE),
            Field(&AutocompleteMatch::contents, u"john d"),
            Field(&AutocompleteMatch::description, u"Aggregator Search"),
            Field(&AutocompleteMatch::destination_url,
                  GURL("https://www.aggregator.com/search?q=john+d"))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxSearchAggregatorHTTPErrorTest,
    ::testing::Values(net::HTTP_BAD_REQUEST,
                      net::HTTP_FORBIDDEN,
                      net::HTTP_NOT_FOUND,
                      net::HTTP_METHOD_NOT_ALLOWED,
                      net::HTTP_INTERNAL_SERVER_ERROR,
                      net::HTTP_BAD_GATEWAY),
    [](const testing::TestParamInfo<net::HttpStatusCode>& info) {
      return base::NumberToString(info.param);
    });
