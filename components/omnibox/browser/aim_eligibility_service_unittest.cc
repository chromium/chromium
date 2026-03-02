// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
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
        response, AimEligibilityService::EligibilityResponseSource::kUser);
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

  MOCK_METHOD(std::string, GetCountryCode, (), (const, override));
  MOCK_METHOD(std::string, GetLocale, (), (const, override));

  void SetAimEligibilityResponse(omnibox::AimEligibilityResponse response) {
    AimEligibilityServiceFriend::UpdateMostRecentResponse(this, response);
  }
};

omnibox::AimEligibilityResponse::QueryParam CreateRequiredParam(
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
  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MockAimEligibilityServiceForInterception>
      aim_eligibility_service_;
};

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_Match) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1&b=2");

  EXPECT_TRUE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MissingParam) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_NoParams) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MultipleRules) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule1;
  rule1.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule1.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule1));

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule2;
  rule2.mutable_required_params()->Add(CreateRequiredParam("c", "1"));
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

TEST_F(AimEligibilityServiceTest, ClientLocaleParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kAimServerEligibilityIncludeClientLocale,
      {{"mode", "get_with_locale"}});

  // Set the locale.
  EXPECT_CALL(*aim_eligibility_service_, GetLocale())
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

  EXPECT_CALL(*aim_eligibility_service_, GetLocale())
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

  EXPECT_CALL(*aim_eligibility_service_, GetLocale())
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

  EXPECT_CALL(*aim_eligibility_service_, GetLocale())
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
  feature_list.InitAndEnableFeature(
      omnibox::kAimCoBrowseEligibilityCheckEnabled);

  omnibox::AimEligibilityResponse response;
  response.set_is_cobrowse_eligible(true);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseEligible());

  omnibox::AimEligibilityResponse response2;
  response2.set_is_cobrowse_eligible(false);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response2));
  EXPECT_FALSE(aim_eligibility_service_->IsCobrowseEligible());
}

TEST_F(AimEligibilityServiceTest, FetchEligibility) {
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
}

TEST_F(AimEligibilityServiceTest, IsCobrowseEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kAimCoBrowseEligibilityCheckEnabled);

  omnibox::AimEligibilityResponse response;
  response.set_is_cobrowse_eligible(false);
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  // Should be true regardless of response if feature is disabled.
  EXPECT_TRUE(aim_eligibility_service_->IsCobrowseEligible());
}

TEST_F(AimEligibilityServiceTest, ParsingResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kAimEnabled, omnibox::kAimServerEligibilityEnabled}, {});

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

  // 2. Trigger a request with another source (e.g. kUser). Header SHOULD NOT be
  // present.
  test_url_loader_factory_.pending_requests()->clear();
  aim_eligibility_service_->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request2 =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  EXPECT_FALSE(request2.headers.HasHeader("User-Agent"));
}
