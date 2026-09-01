// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/host_override.h"
#include "components/country_codes/country_codes.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_eligibility_client_request.pb.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

using testing::ReturnRef;

namespace content {
class WebContents;
}  // namespace content

class AimEligibilityServiceFriend {
 public:
  static void UpdateMostRecentResponse(
      AimEligibilityService* service,
      const omnibox::AimEligibilityResponse& response) {
    service->UpdateMostRecentResponse(
        response, AimEligibilityService::EligibilityResponseSource::kUser,
        AimEligibilityService::AuthenticationMethod::kNone);
  }

  static bool IsIetfBcp47(const std::string& locale) {
    return AimEligibilityService::IsIetfBcp47(locale);
  }
};

namespace {
// A mock AimEligibilityService that provides a mock response for member
// functions to use.
class MockAimEligibilityServiceForInterception : public AimEligibilityService {
 public:
  MockAimEligibilityServiceForInterception(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Configuration configuration = {})
      : AimEligibilityService(pref_service,
                              template_url_service,
                              std::move(url_loader_factory),
                              nullptr,
                              "en-US",
                              std::move(configuration)) {}
  ~MockAimEligibilityServiceForInterception() override = default;

  MOCK_METHOD(std::string, GetLocaleImpl, (), (const, override));

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

  void SetAimEligibilityResponse(omnibox::AimEligibilityResponse response) {
    AimEligibilityServiceFriend::UpdateMostRecentResponse(this, response);
  }
};

omnibox::AimEligibilityResponse::QueryParam CreateQueryParam(
    const std::string& key,
    const std::string& value) {
  omnibox::AimEligibilityResponse::QueryParam param;
  param.set_key(key);
  param.set_value(value);
  return param;
}

}  // namespace

class AimEligibilityServiceTest : public testing::Test {
 public:
  explicit AimEligibilityServiceTest() {}

  void SetUp() override {
    AimEligibilityService::RegisterProfilePrefs(
        search_engines_test_environment_.pref_service().registry());
    CreateService();
  }

  void CreateService(
      const AimEligibilityService::Configuration& configuration = {}) {
    aim_eligibility_service_ =
        std::make_unique<MockAimEligibilityServiceForInterception>(
            search_engines_test_environment_.pref_service(),
            search_engines_test_environment_.template_url_service(),
            test_url_loader_factory_.GetSafeWeakWrapper(), configuration);
  }

  void TearDown() override { aim_eligibility_service_ = nullptr; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MockAimEligibilityServiceForInterception>
      aim_eligibility_service_;
};

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_Match) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1&b=2");

  EXPECT_TRUE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MissingParam) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_NoParams) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MultipleRules) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule1;
  rule1.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule1.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule1));

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule2;
  rule2.mutable_required_params()->Add(CreateQueryParam("c", "1"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule2));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?c=1");

  EXPECT_TRUE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_NoRules) {
  omnibox::AimEligibilityResponse response;
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?c=1");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, IsAimUrl) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;
  rule.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  response.mutable_interception_allowed_hosts()->Add("google.com");
  response.mutable_interception_allowed_hosts()->Add("example.com");

  response.mutable_interception_allowed_paths()->Add("/search");
  response.mutable_interception_allowed_paths()->Add("/search_dev");

  // Add a "no cobrowse" param to make sure this IsAimUrl is not affected by it.
  response.mutable_no_cobrowse_params()->Add(CreateQueryParam("ncb", "1"));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // Matches all criteria
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com/search?a=1&b=2"), std::nullopt));

  // Matches all criteria, second host and path
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://example.com/search_dev?a=1&b=2"), std::nullopt));

  // Missing param
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com/search?a=1"), std::nullopt));

  // Invalid host
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://host.google.com/search?a=1&b=2"), std::nullopt));

  // Invalid path
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com/feature?a=1&b=2"), std::nullopt));

  // Check that the host override works correctly with and without ports
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://goo.gl/feature?a=1&b=2"),
      contextual_tasks::HostOverride::FromString("goo.gl")));
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://goo.gl/search?a=1&b=2"),
      contextual_tasks::HostOverride::FromString("goo.gl")));
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://goo.gl:8888/search?a=1&b=2"),
      contextual_tasks::HostOverride::FromString("goo.gl:8888")));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://goo.gl:9999/search?a=1&b=2"),
      contextual_tasks::HostOverride::FromString("goo.gl:8888")));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://goo.gl/search?a=1&b=2"),
      contextual_tasks::HostOverride::FromString("goo.gl:8888")));
}

TEST_F(AimEligibilityServiceTest, IsAimHost_HostOverrideWithPort) {
  contextual_tasks::HostOverride override_with_port{"localhost.corp.google.com",
                                                    8888};
  EXPECT_TRUE(aim_eligibility_service_->IsAimHost(
      GURL("https://localhost.corp.google.com:8888/search"),
      override_with_port));
  EXPECT_FALSE(aim_eligibility_service_->IsAimHost(
      GURL("https://localhost.corp.google.com:9999/search"),
      override_with_port));
  EXPECT_FALSE(aim_eligibility_service_->IsAimHost(
      GURL("https://localhost.corp.google.com/search"), override_with_port));

  contextual_tasks::HostOverride override_no_port{"localhost.corp.google.com",
                                                  std::nullopt};
  EXPECT_TRUE(aim_eligibility_service_->IsAimHost(
      GURL("https://localhost.corp.google.com/search"), override_no_port));
  EXPECT_FALSE(aim_eligibility_service_->IsAimHost(
      GURL("https://localhost.corp.google.com:8888/search"), override_no_port));
}

TEST_F(AimEligibilityServiceTest, IsAimUrl_HostWildcard) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;
  rule.mutable_required_params()->Add(CreateQueryParam("a", "1"));
  rule.mutable_required_params()->Add(CreateQueryParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  response.mutable_interception_allowed_hosts()->Add("google.com");
  response.mutable_interception_allowed_hosts()->Add("*.google.com");

  response.mutable_interception_allowed_paths()->Add("/search");

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // No subdomain for wildcard
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com/search?a=1&b=2"), std::nullopt));

  // Valid subdomain
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://prod.google.com/search?a=1&b=2"), std::nullopt));

  // Invalid host
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.example.com/search?a=1&b=2"), std::nullopt));
}

TEST_F(AimEligibilityServiceTest, IsAimUrl_SecurityNearDomains) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;
  rule.mutable_required_params()->Add(CreateQueryParam("udm", "50"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  response.mutable_interception_allowed_hosts()->Add("www.google.com");
  response.mutable_interception_allowed_hosts()->Add("google.com");

  response.mutable_interception_allowed_paths()->Add("/search");

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // Legitimate domains match.
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://www.google.com/search?udm=50"), std::nullopt));
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com/search?udm=50"), std::nullopt));
  EXPECT_TRUE(aim_eligibility_service_->IsAimUrl(
      GURL("https://WWW.GOOGLE.COM/search?udm=50"), std::nullopt));

  // Attacker-controlled near-domains and similar strings MUST NOT match.
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://www0google.com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://wwwagoogle.com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://www-google.com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google0com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://googleicom/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://fakegoogle.com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://google.com.evil.com/search?udm=50"), std::nullopt));
  EXPECT_FALSE(aim_eligibility_service_->IsAimUrl(
      GURL("https://notgoogle.com/search?udm=50"), std::nullopt));
}

TEST_F(AimEligibilityServiceTest, HasNoCobrowseParams_ExactMatch) {
  omnibox::AimEligibilityResponse response;
  response.mutable_no_cobrowse_params()->Add(CreateQueryParam("ncb", "1"));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?ncb=1");

  EXPECT_TRUE(aim_eligibility_service_->HasNoCobrowseParams(url));
}

// Checking for no-cobrowse params should be a "contains" check rather than an
// exact match.
TEST_F(AimEligibilityServiceTest, HasNoCobrowseParams_Contains) {
  omnibox::AimEligibilityResponse response;
  response.mutable_no_cobrowse_params()->Add(
      CreateQueryParam("deb", "nocobrowse1"));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?deb=debug0nocobrowse1other0");

  EXPECT_TRUE(aim_eligibility_service_->HasNoCobrowseParams(url));
}

// Check `false` is returned if the params aren't present.
TEST_F(AimEligibilityServiceTest, HasNoCobrowseParams_None) {
  omnibox::AimEligibilityResponse response;
  response.mutable_no_cobrowse_params()->Add(
      CreateQueryParam("deb", "nocobrowse1"));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?deb=0");

  EXPECT_FALSE(aim_eligibility_service_->HasNoCobrowseParams(url));
}

TEST_F(AimEligibilityServiceTest, ClientLocaleParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAimServerEligibilityIncludeClientLocale,
      {{"mode", "get_with_locale"}});

  // Set the locale.
  EXPECT_CALL(*aim_eligibility_service_, GetLocaleImpl())
      .WillRepeatedly(testing::Return("es-419"));

  // Trigger the request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  // Verify that the request URL contains the client_locale query param.
  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  std::string value;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request->url, "client_locale", &value));
  EXPECT_EQ(value, "es-419");
}

TEST_F(AimEligibilityServiceTest, ClientCountryParam_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      omnibox::kAimServerEligibilityIncludeClientCountry);

  // Trigger the request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  // Verify that the request URL contains the client_country query param falling
  // back to country_codes.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  std::string value;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request.url, "client_country", &value));
  std::string expected_country{
      country_codes::GetCurrentCountryID().CountryCode()};
  // Check if this environment has a CountryCode() defined as "zz", since that
  // country code should be capitalized. All other country codes are lowercase.
  if (base::EqualsCaseInsensitiveASCII(expected_country, "zz")) {
    EXPECT_EQ(value, "ZZ");
  } else {
    EXPECT_EQ(value, base::ToLowerASCII(expected_country));
  }
}

TEST_F(AimEligibilityServiceTest, ClientCountryParam_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kAimServerEligibilityIncludeClientCountry);

  // Trigger the request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  // Verify that the request URL does NOT contain the client_country query
  // param.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  std::string value;
  EXPECT_FALSE(
      net::GetValueForKeyInQuery(request.url, "client_country", &value));
}

TEST_F(AimEligibilityServiceTest, UdmParamAppended) {
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(request->url, "udm", &value));
  EXPECT_EQ(value, "50");
}

TEST_F(AimEligibilityServiceTest, RequestMode_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kAimServerEligibilityIncludeClientLocale);

  EXPECT_EQ(AimEligibilityService::GetServerEligibilityRequestMode(),
            AimEligibilityService::ServerEligibilityRequestMode::kLegacyGet);

  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  EXPECT_EQ(request->method, "GET");
  std::string value;
  // Legacy GET (disabled) should NOT have client_locale.
  EXPECT_FALSE(
      net::GetValueForKeyInQuery(request->url, "client_locale", &value));
}

TEST_F(AimEligibilityServiceTest, RequestMode_GetWithLocale) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAimServerEligibilityIncludeClientLocale,
      {{"mode", "get_with_locale"}});

  EXPECT_CALL(*aim_eligibility_service_, GetLocaleImpl())
      .WillRepeatedly(testing::Return("es-419"));

  EXPECT_EQ(
      AimEligibilityService::GetServerEligibilityRequestMode(),
      AimEligibilityService::ServerEligibilityRequestMode::kGetWithLocale);

  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  EXPECT_EQ(request->method, "GET");
  std::string value;
  // GET with Locale SHOULD have client_locale.
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request->url, "client_locale", &value));
  EXPECT_EQ(value, "es-419");
}

TEST_F(AimEligibilityServiceTest, RequestMode_EnabledDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      omnibox::kAimServerEligibilityIncludeClientLocale);

  EXPECT_CALL(*aim_eligibility_service_, GetLocaleImpl())
      .WillRepeatedly(testing::Return("es-419"));

  // Default when enabled without params is now GetWithLocale.
  EXPECT_EQ(
      AimEligibilityService::GetServerEligibilityRequestMode(),
      AimEligibilityService::ServerEligibilityRequestMode::kGetWithLocale);

  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  EXPECT_EQ(request->method, "GET");
  std::string value;
  // GET with Locale SHOULD have client_locale.
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request->url, "client_locale", &value));
  EXPECT_EQ(value, "es-419");
}

TEST_F(AimEligibilityServiceTest, RequestMode_PostWithProto) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAimServerEligibilityIncludeClientLocale,
      {{"mode", "post_with_proto"}});

  EXPECT_CALL(*aim_eligibility_service_, GetLocaleImpl())
      .WillRepeatedly(testing::Return("es-419"));

  EXPECT_EQ(
      AimEligibilityService::GetServerEligibilityRequestMode(),
      AimEligibilityService::ServerEligibilityRequestMode::kPostWithProto);

  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  const network::ResourceRequest* request =
      &test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_TRUE(request);
  EXPECT_EQ(request->method, "POST");
  std::string value;
  // POST with Proto should NOT have client_locale in query params.
  EXPECT_FALSE(
      net::GetValueForKeyInQuery(request->url, "client_locale", &value));

  // Verify body contains proto.
  std::string body = network::GetUploadData(*request);
  omnibox::AimEligibilityClientRequest client_request;
  EXPECT_TRUE(client_request.ParseFromString(body));
  EXPECT_EQ(client_request.client_locale(), "es-419");
}

TEST_F(AimEligibilityServiceTest, IsCobrowseEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kAimCoBrowseEligibilityCheckEnabled,
                                 contextual_tasks::kContextualTasks},
                                {});

  omnibox::AimEligibilityResponse response;
  response.set_is_cobrowse_eligible(true);
  response.set_is_eligible(true);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseEligible());
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseServerEligible());

  omnibox::AimEligibilityResponse response2;
  response2.set_is_cobrowse_eligible(false);
  response.set_is_eligible(true);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response2));
  EXPECT_FALSE(aim_eligibility_service_->IsCobrowseEligible());
  EXPECT_FALSE(aim_eligibility_service_->IsCobrowseServerEligible());
}

TEST_F(AimEligibilityServiceTest, FetchEligibility) {
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
}

TEST_F(AimEligibilityServiceTest, ManualOverrideBlocksAutomaticRequests) {
  // Initial state: No override. Automatic request should go through.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Clean up pending request.
  test_url_loader_factory_.pending_requests()->clear();

  // Set manual override.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  std::string response_string;
  response.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);
  EXPECT_TRUE(aim_eligibility_service_->SetEligibilityResponseForDebugging(
      encoded_response));

  // Automatic request should now be blocked.
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Re-create the service to simulate a browser restart.
  CreateService();

  // Automatic request should still be blocked after "restart".
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Manual request (kUser) should still go through.
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // After starting a manual request, the override should be cleared,
  // so subsequent automatic requests should go through.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
}

TEST_F(AimEligibilityServiceTest, ManualOverrideExpires) {
  // Set manual override.
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  std::string response_string;
  response.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);
  EXPECT_TRUE(aim_eligibility_service_->SetEligibilityResponseForDebugging(
      encoded_response));

  // Automatic request should now be blocked.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Fast forward time by 23 hours. Should still be blocked.
  task_environment_.FastForwardBy(base::Hours(23));
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Fast forward time by another 2 hours (total 25 hours, > 24 hours).
  // The override should expire and the request should go through.
  task_environment_.FastForwardBy(base::Hours(2));
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
}

TEST_F(AimEligibilityServiceTest, IsCobrowseEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({contextual_tasks::kContextualTasks},
                                {omnibox::kAimCoBrowseEligibilityCheckEnabled,
                                 omnibox::kAimServerEligibilityEnabled});

  omnibox::AimEligibilityResponse response;
  response.set_is_cobrowse_eligible(false);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // Should be true regardless of response if feature is disabled.
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseEligible());
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseServerEligible());
}

TEST_F(AimEligibilityServiceTest, IsCobrowseEligible_ContextualTasksDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kAimCoBrowseEligibilityCheckEnabled},
                                {contextual_tasks::kContextualTasks});

  omnibox::AimEligibilityResponse response;
  response.set_is_cobrowse_eligible(true);
  response.set_is_eligible(true);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));
  EXPECT_FALSE(aim_eligibility_service_->IsCobrowseEligible());
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseServerEligible());
}

TEST_F(AimEligibilityServiceTest, ParsingResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kAimEnabled, omnibox::kAimServerEligibilityEnabled,
       contextual_tasks::kContextualTasks},
      {});

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  response.set_is_cobrowse_eligible(true);

  std::string response_string;
  response.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);

  EXPECT_TRUE(aim_eligibility_service_->SetEligibilityResponseForDebugging(
      encoded_response));
  EXPECT_TRUE(aim_eligibility_service_->IsAimEligible());
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseEligible());
}

TEST_F(AimEligibilityServiceTest, FullVersionListHeader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      omnibox::kAimServerEligibilitySendFullVersionListEnabled);

  AimEligibilityService::Configuration config;
  config.full_version_list = "Test Brand List";
  CreateService(config);

  // Trigger a request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::optional<std::string> header_value =
      request.headers.GetHeader("Sec-CH-UA-Full-Version-List");
  EXPECT_TRUE(header_value.has_value());
  EXPECT_EQ(*header_value, "Test Brand List");
}

TEST_F(AimEligibilityServiceTest, FullVersionListHeader_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kAimServerEligibilitySendFullVersionListEnabled);

  AimEligibilityService::Configuration config;
  config.full_version_list = "Test Brand List";
  CreateService(config);

  // Trigger a request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_FALSE(request.headers.HasHeader("Sec-CH-UA-Full-Version-List"));
}

TEST_F(AimEligibilityServiceTest, CoBrowseUserAgentSuffix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      omnibox::kAimServerEligibilitySendCoBrowseUserAgentSuffixEnabled);

  AimEligibilityService::Configuration config;
  config.user_agent_with_cobrowse_suffix = "UA with Suffix";
  CreateService(config);

  // 1. Trigger a request with source kAimUrlNavigation. Header SHOULD be
  // present.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::optional<std::string> ua_value = request.headers.GetHeader("User-Agent");
  EXPECT_TRUE(ua_value.has_value());
  EXPECT_EQ(*ua_value, "UA with Suffix");

  // 2. Trigger a request with another source (e.g. kUser). Header SHOULD also
  // be present.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request2 =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::optional<std::string> ua_value2 =
      request2.headers.GetHeader("User-Agent");
  EXPECT_TRUE(ua_value2.has_value());
  EXPECT_EQ(*ua_value2, "UA with Suffix");
}

TEST_F(AimEligibilityServiceTest, IsFuseboxEligible_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      omnibox::kAimFuseboxEligibilityCheckEnabled);

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  response.set_is_fusebox_eligible(true);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));
  EXPECT_TRUE(aim_eligibility_service_->IsFuseboxEligible());

  omnibox::AimEligibilityResponse response2;
  response2.set_is_eligible(true);
  response2.set_is_fusebox_eligible(false);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response2));
  EXPECT_FALSE(aim_eligibility_service_->IsFuseboxEligible());
}

TEST_F(AimEligibilityServiceTest, IsFuseboxEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {omnibox::kAimFuseboxEligibilityCheckEnabled,
                                 omnibox::kAimServerEligibilityEnabled});

  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  response.set_is_fusebox_eligible(false);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // Should be true regardless of response if feature is disabled.
  EXPECT_TRUE(aim_eligibility_service_->IsFuseboxEligible());
}

TEST_F(AimEligibilityServiceTest, IsIetfBcp47) {
  // Valid BCP 47 strings (no underscores)
  EXPECT_TRUE(AimEligibilityServiceFriend::IsIetfBcp47("en"));
  EXPECT_TRUE(AimEligibilityServiceFriend::IsIetfBcp47("en-US"));
  EXPECT_TRUE(AimEligibilityServiceFriend::IsIetfBcp47("es-419"));
  EXPECT_TRUE(AimEligibilityServiceFriend::IsIetfBcp47("sr-Latn-RS"));

  // Invalid strings (contains underscores)
  EXPECT_FALSE(AimEligibilityServiceFriend::IsIetfBcp47("en_US"));
  EXPECT_FALSE(AimEligibilityServiceFriend::IsIetfBcp47("fr_CA"));
  EXPECT_FALSE(AimEligibilityServiceFriend::IsIetfBcp47("sr_Latn_RS"));
}

TEST_F(AimEligibilityServiceTest, LogsFuseboxEligibilityHistogram) {
  base::HistogramTester histogram_tester;
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);
  response.set_is_fusebox_eligible(true);

  // Trigger the request.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  // Respond to the pending request.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::string response_string;
  response.SerializeToString(&response_string);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request.url.spec(), response_string, net::HTTP_OK);

  // Verify histograms.
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.User.is_fusebox_eligible",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.is_fusebox_eligible", true,
      1);
}

TEST_F(AimEligibilityServiceTest, FetchEligibilityWithLocaleChange) {
  base::HistogramTester histogram_tester;
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kLocaleChange);

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::string response_string;
  response.SerializeToString(&response_string);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request.url.spec(), response_string, net::HTTP_OK);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.LocaleChange.is_eligible",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.is_eligible", true, 1);
}
