// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider.h"

#include <map>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/proto/site_token_data.pb.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_token_provider {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

constexpr char kUsersMeResourceName[] = "users/me";

// Mock version of the core SiteTokenProvider engine for testing.
class MockSiteTokenProvider : public SiteTokenProvider {
 public:
  MockSiteTokenProvider() = default;
  ~MockSiteTokenProvider() override = default;

  MOCK_METHOD(void,
              SetTokenUpdateCallback,
              (TokenUpdateCallback callback),
              (override));
  MOCK_METHOD(void, UpdateState, (), (override));
};

class SiteTokenProviderServiceTest : public ::testing::Test {
 protected:
  SiteTokenProviderServiceTest() {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteTokenProviderEnabled);
  }

  ~SiteTokenProviderServiceTest() override = default;

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProviderServiceTest, UpdatesStateOnStartupIfSignedIn) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto mock_provider = std::make_unique<StrictMock<MockSiteTokenProvider>>();
  MockSiteTokenProvider* mock_ptr = mock_provider.get();
  EXPECT_CALL(*mock_ptr, SetTokenUpdateCallback(_)).Times(1);
  EXPECT_CALL(*mock_ptr, UpdateState()).Times(1);
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));
  service.Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, UpdatesStateOnSignInEvent) {
  auto mock_provider = std::make_unique<StrictMock<MockSiteTokenProvider>>();
  MockSiteTokenProvider* mock_ptr = mock_provider.get();
  EXPECT_CALL(*mock_ptr, SetTokenUpdateCallback(_)).Times(1);
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));
  EXPECT_CALL(*mock_ptr, UpdateState()).Times(1);
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  service.Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, IsDomainAllowlisted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSiteTokenProviderEnabled,
      {{"site_token_allowlist", "example.com,www.test.com"}});

  auto mock_provider = std::make_unique<NiceMock<MockSiteTokenProvider>>();
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));

  EXPECT_TRUE(service.IsDomainAllowlisted("example.com"));
  EXPECT_TRUE(service.IsDomainAllowlisted("www.example.com"));
  EXPECT_TRUE(service.IsDomainAllowlisted("test.com"));
  EXPECT_TRUE(service.IsDomainAllowlisted("www.test.com"));
  EXPECT_FALSE(service.IsDomainAllowlisted("other.com"));
  EXPECT_FALSE(service.IsDomainAllowlisted("sub.example.com"));

  service.Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, EmptyAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSiteTokenProviderEnabled, {{"site_token_allowlist", ""}});

  auto mock_provider = std::make_unique<NiceMock<MockSiteTokenProvider>>();
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));

  EXPECT_FALSE(service.IsDomainAllowlisted("example.com"));

  service.Shutdown();
}

class SiteTokenProviderServiceDomainTest
    : public SiteTokenProviderServiceTest,
      public ::testing::WithParamInterface<
          std::pair<std::string, std::string>> {};

TEST_P(SiteTokenProviderServiceDomainTest,
       ReturnsTokensForMatchingDomainsOnly) {
  auto mock_provider = std::make_unique<NiceMock<MockSiteTokenProvider>>();
  MockSiteTokenProvider* mock_ptr = mock_provider.get();

  SiteTokenProvider::TokenUpdateCallback update_callback;
  EXPECT_CALL(*mock_ptr, SetTokenUpdateCallback(_))
      .WillOnce(::testing::SaveArg<0>(&update_callback));
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));
  ASSERT_TRUE(update_callback);

  // Populate cache.
  std::map<std::string, std::string> mock_tokens = {
      {"www.site1.com", "token-site-1"},
      {"site2.com", "token-site-2"},
      {"site3.com", "token-site-3"}};
  update_callback.Run(mock_tokens);

  const auto& [query_domain, expected_token] = GetParam();
  EXPECT_EQ(service.GetTokenForDomain(query_domain), expected_token);

  service.Shutdown();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteTokenProviderServiceDomainTest,
    ::testing::Values(std::make_pair("site1.com", "token-site-1"),
                      std::make_pair("site3.com", "token-site-3"),
                      std::make_pair("www.site1.com", "token-site-1"),
                      std::make_pair("www.site3.com", "token-site-3"),
                      std::make_pair("site2.com", "token-site-2"),
                      std::make_pair("secondary.site1.com", ""),
                      std::make_pair("google.com", "")));

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SiteTokenProviderServiceTest, ClearsCacheOnSignOut) {
  auto mock_provider = std::make_unique<NiceMock<MockSiteTokenProvider>>();
  MockSiteTokenProvider* mock_ptr = mock_provider.get();

  SiteTokenProvider::TokenUpdateCallback update_callback;
  EXPECT_CALL(*mock_ptr, SetTokenUpdateCallback(_))
      .WillOnce(::testing::SaveArg<0>(&update_callback));
  SiteTokenProviderService service(identity_test_env_.identity_manager(),
                                   std::move(mock_provider));
  ASSERT_TRUE(update_callback);

  std::map<std::string, std::string> mock_tokens = {
      {"site1.com", "token-site-1"}};
  update_callback.Run(mock_tokens);
  EXPECT_EQ(service.GetTokenForDomain("site1.com"), "token-site-1");

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service.GetTokenForDomain("site1.com"), "");

  service.Shutdown();
}
#endif

class SiteTokenProviderTest : public ::testing::Test {
 protected:
  SiteTokenProviderTest() {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"oauth2_scope", "test_scope"},
         {"site_token_endpoint_url", "https://example.com/fetch"}});
  }
  ~SiteTokenProviderTest() override = default;

  std::unique_ptr<SiteTokenProvider> CreateProvider(
      SiteTokenProvider::TokenUpdateCallback callback) {
    auto provider = SiteTokenProvider::Create(
        identity_test_env_.identity_manager(), shared_url_loader_factory_);
    provider->SetTokenUpdateCallback(std::move(callback));
    return provider;
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProviderTest, FetchesSuccessfully) {
  proto::GeneratePlatformSiteTokensResponse response;
  auto* token = response.add_site_tokens();
  token->set_domain("site-1");
  token->set_token("token-value-123");

  std::string response_data;
  ASSERT_TRUE(response.SerializeToString(&response_data));

  test_url_loader_factory_.AddResponse("https://example.com/fetch",
                                       response_data, net::HTTP_OK);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        proto::GeneratePlatformSiteTokensRequest request_proto;
        std::string request_body;
        if (request.request_body &&
            !request.request_body->elements()->empty()) {
          request_body = std::string(request.request_body->elements()
                                         ->at(0)
                                         .As<network::DataElementBytes>()
                                         .AsStringPiece());
        }
        ASSERT_TRUE(request_proto.ParseFromString(request_body));
        EXPECT_EQ(request_proto.name(), kUsersMeResourceName);
      }));

  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

  base::RunLoop run_loop;
  std::map<std::string, std::string> received_tokens;
  auto callback = base::BindLambdaForTesting(
      [&received_tokens, &run_loop](std::map<std::string, std::string> tokens) {
        received_tokens = std::move(tokens);
        run_loop.Quit();
      });

  auto provider = CreateProvider(std::move(callback));
  provider->UpdateState();
  run_loop.Run();

  EXPECT_EQ(received_tokens.size(), 1u);
  EXPECT_EQ(received_tokens["site-1"], "token-value-123");
}

TEST(SiteTokenProviderDomainTest, NormalizeDomain) {
  EXPECT_EQ(NormalizeDomain("example.com"), "example.com");
  EXPECT_EQ(NormalizeDomain("www.example.com"), "example.com");
  EXPECT_EQ(NormalizeDomain("WWW.EXAMPLE.COM"), "example.com");
  EXPECT_EQ(NormalizeDomain("sub.example.com"), "sub.example.com");
  EXPECT_EQ(NormalizeDomain("localhost"), "localhost");
}

TEST(SiteTokenProviderDomainTest, ParseAllowlistedDomains) {
  base::flat_set<std::string> domains = ParseAllowlistedDomains(
      "example.com, www.example2.com , Sub.Example3.Com ,, ");
  EXPECT_EQ(domains.size(), 3u);
  EXPECT_TRUE(domains.contains("example.com"));
  EXPECT_TRUE(domains.contains("example2.com"));
  EXPECT_TRUE(domains.contains("sub.example3.com"));
}

}  // namespace
}  // namespace site_token_provider
