// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_store.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/proto/site_token_data.pb.h"
#include "components/site_token_provider/site_token_provider_prefs.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_token_provider {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::SaveArg;
using ::testing::StrictMock;

using TokenMap = std::map<std::string, std::string>;

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
  struct ServiceContext {
    std::unique_ptr<SiteTokenProviderService> service;
    raw_ptr<MockSiteTokenProvider> mock_provider = nullptr;
    SiteTokenProvider::TokenUpdateCallback update_callback;
  };

  SiteTokenProviderServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteTokenProviderEnabled);
  }

  ~SiteTokenProviderServiceTest() override = default;

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
  }

  // Construction fetches tokens when signed in, so expect that call up front.
  ServiceContext CreateService(bool expect_startup_update) {
    ServiceContext ctx;
    auto mock = std::make_unique<StrictMock<MockSiteTokenProvider>>();
    ctx.mock_provider = mock.get();
    EXPECT_CALL(*ctx.mock_provider, SetTokenUpdateCallback(_))
        .WillOnce(SaveArg<0>(&ctx.update_callback));
    if (expect_startup_update) {
      EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
    }
    ctx.service = std::make_unique<SiteTokenProviderService>(
        identity_test_env_.identity_manager(), std::move(mock));
    CHECK(ctx.update_callback);
    return ctx;
  }

  ServiceContext CreateServiceWithAccount() {
    SignIn();
    return CreateService(/*expect_startup_update=*/true);
  }

  base::TimeDelta RefreshInterval() {
    return features::kSiteTokenRefreshInterval.Get();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProviderServiceTest, UpdatesStateOnStartupIfSignedIn) {
  auto ctx = CreateServiceWithAccount();
  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, UpdatesStateOnSignIn) {
  auto ctx = CreateService(/*expect_startup_update=*/false);
  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  SignIn();
  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, RefreshesPeriodically) {
  auto ctx = CreateServiceWithAccount();
  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});
  EXPECT_EQ(ctx.service->GetTokenForDomain("example.com"), "token1");

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(0);
  task_environment_.FastForwardBy(RefreshInterval() - base::Seconds(1));
  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  task_environment_.FastForwardBy(base::Seconds(1));

  ctx.update_callback.Run(TokenMap{{"example.com", "token2"}});
  EXPECT_EQ(ctx.service->GetTokenForDomain("example.com"), "token2");
  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(0);
  task_environment_.FastForwardBy(RefreshInterval() - base::Seconds(1));
  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  task_environment_.FastForwardBy(base::Seconds(1));

  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, KeepsCacheAndRefreshAfterFailedFetch) {
  auto ctx = CreateServiceWithAccount();
  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  task_environment_.FastForwardBy(RefreshInterval());

  ctx.update_callback.Run(std::nullopt);
  EXPECT_EQ(ctx.service->GetTokenForDomain("example.com"), "token1");

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  task_environment_.FastForwardBy(RefreshInterval());

  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, ClearsCacheOnEmptySuccessfulFetch) {
  auto ctx = CreateServiceWithAccount();
  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});

  ctx.update_callback.Run(TokenMap());
  EXPECT_TRUE(ctx.service->GetTokenForDomain("example.com").empty());

  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, DoesNotScheduleTimerWhenIntervalIsZero) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSiteTokenProviderEnabled,
      {{"site_token_refresh_interval", "0s"}});

  auto ctx = CreateServiceWithAccount();
  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(0);
  task_environment_.FastForwardBy(base::Days(14));

  ctx.service->Shutdown();
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
  auto ctx = CreateServiceWithAccount();

  // Populate cache.
  ctx.update_callback.Run(TokenMap{{"www.site1.com", "token-site-1"},
                                   {"site2.com", "token-site-2"},
                                   {"site3.com", "token-site-3"}});

  const auto& [query_domain, expected_token] = GetParam();
  EXPECT_EQ(ctx.service->GetTokenForDomain(query_domain), expected_token);

  ctx.service->Shutdown();
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
TEST_F(SiteTokenProviderServiceTest, SignOutClearsCacheAndStopsRefresh) {
  auto ctx = CreateServiceWithAccount();
  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});
  EXPECT_EQ(ctx.service->GetTokenForDomain("example.com"), "token1");

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_TRUE(ctx.service->GetTokenForDomain("example.com").empty());

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(0);
  task_environment_.FastForwardBy(RefreshInterval());

  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, InFlightTokenUpdateIgnoredIfSignedOut) {
  auto ctx = CreateServiceWithAccount();

  identity_test_env_.ClearPrimaryAccount();

  ctx.update_callback.Run(TokenMap{{"example.com", "token_stale"}});
  EXPECT_TRUE(ctx.service->GetTokenForDomain("example.com").empty());

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(0);
  task_environment_.FastForwardBy(RefreshInterval());

  ctx.service->Shutdown();
}

TEST_F(SiteTokenProviderServiceTest, CachesTokensAfterSignOutAndSignIn) {
  auto ctx = CreateServiceWithAccount();
  base::WeakPtr<SiteTokenProviderService> weak_service =
      ctx.service->GetWeakPtr();

  identity_test_env_.ClearPrimaryAccount();

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  SignIn();

  ctx.update_callback.Run(TokenMap{{"example.com", "token1"}});
  EXPECT_EQ(ctx.service->GetTokenForDomain("example.com"), "token1");

  // SiteTokenHeaderClient holds a weak pointer to the service.
  EXPECT_TRUE(weak_service);

  EXPECT_CALL(*ctx.mock_provider, UpdateState()).Times(1);
  task_environment_.FastForwardBy(RefreshInterval());

  ctx.service->Shutdown();
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

  // Signs in, runs one fetch to completion and returns the callback's result.
  std::optional<TokenMap> FetchTokens() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    base::RunLoop run_loop;
    std::optional<TokenMap> received_tokens;
    auto provider = CreateProvider(base::BindLambdaForTesting(
        [&received_tokens, &run_loop](std::optional<TokenMap> tokens) {
          received_tokens = std::move(tokens);
          run_loop.Quit();
        }));
    provider->UpdateState();
    run_loop.Run();
    return received_tokens;
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

  std::optional<TokenMap> received_tokens = FetchTokens();
  ASSERT_TRUE(received_tokens.has_value());
  EXPECT_THAT(*received_tokens, ElementsAre(Pair("site-1", "token-value-123")));
}

TEST_F(SiteTokenProviderTest, ReportsFailureOnServerError) {
  test_url_loader_factory_.AddResponse("https://example.com/fetch", "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_EQ(FetchTokens(), std::nullopt);
}

TEST_F(SiteTokenProviderTest, ReportsFailureOnUnparseableResponse) {
  test_url_loader_factory_.AddResponse("https://example.com/fetch",
                                       "not a proto", net::HTTP_OK);

  EXPECT_EQ(FetchTokens(), std::nullopt);
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

TEST(SiteTokenProviderPrefsTest, RegisterProfilePrefs) {
  auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  site_token_provider::prefs::RegisterProfilePrefs(registry.get());

  const base::Value* default_value = nullptr;
  ASSERT_TRUE(registry->defaults()->GetValue(prefs::kSiteTokenProviderPref,
                                             &default_value));
  EXPECT_EQ(*default_value, base::Value(false));

  EXPECT_EQ(registry->GetRegistrationFlags(prefs::kSiteTokenProviderPref),
            user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

}  // namespace
}  // namespace site_token_provider
