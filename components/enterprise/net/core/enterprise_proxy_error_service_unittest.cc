// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/auth.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

namespace {

constexpr char kAuthBlockJson[] = R"(,
          "google_chrome": {
            "auth": {
              "type": "PROFILE_BEARER_TOKEN",
              "scope": "CLOUD_SECURE_GATEWAY"
            }
          })";

constexpr char kPvdConfigJsonTemplate[] = R"({
      "proxies": [
        {
          "protocol": "https-connect",
          "identity": "proxy1",
          "proxy": "https://%s:443"%s
        }
      ],
      "proxy-match": [
        {
          "proxies": ["proxy1"],
          "domains": ["*.securegateway.com"]
        }
      ]
    })";

base::DictValue CreateDomainPolicyEntry(const std::string& pvd_id,
                                        bool use_oauth) {
  base::DictValue entry;
  entry.Set("pvd_id", pvd_id);
  base::DictValue auth;
  auth.Set("type", use_oauth ? "profile_bearer_token" : "none");
  if (use_oauth) {
    auth.Set("scope", "cloud_secure_gateway");
  }
  entry.Set("auth_config", std::move(auth));
  return entry;
}

}  // namespace

class EnterpriseProxyErrorServiceTest : public testing::Test {
 public:
  EnterpriseProxyErrorServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(kEnableDynamicRouteFetching);
  }

  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());

    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "user@managed.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "managed.com", "Full Name", "Given Name", "en-US", "picture_url");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());

    auth_service_ = std::make_unique<EnterpriseNetworkAuthService>(
        identity_test_env_.identity_manager(), &pref_service_,
        &profile_id_service_);

    auto callback = base::BindRepeating(
        [](network::TestURLLoaderFactory* factory)
            -> scoped_refptr<network::SharedURLLoaderFactory> {
          return factory->GetSafeWeakWrapper();
        },
        base::Unretained(test_url_loader_factory_.get()));

    proxy_service_ = std::make_unique<EnterpriseProxyService>(
        &pref_service_, auth_service_.get(), std::move(callback),
        &profile_id_service_);

    error_service_ =
        std::make_unique<EnterpriseProxyErrorService>(proxy_service_.get());
  }

  void TearDown() override {
    error_service_.reset();
    proxy_service_.reset();
    auth_service_.reset();
    shared_url_loader_factory_.reset();
    test_url_loader_factory_.reset();
  }

  void SetupManagedDomainWithProxy(std::string_view proxy_host,
                                   bool with_auth = true) {
    base::ListValue policy_domains;
    policy_domains.Append(CreateDomainPolicyEntry("secure-gateway.com", true));
    pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));

    std::string auth_block = with_auth ? kAuthBlockJson : "";
    std::string pvd_config_json =
        base::StringPrintf(kPvdConfigJsonTemplate,
                           std::string(proxy_host).c_str(), auth_block.c_str());

    test_url_loader_factory_->SimulateResponseForPendingRequest(
        "https://secure-gateway.com/.well-known/pvd", pvd_config_json);
  }

  net::AuthChallengeInfo CreateProxyAuthChallengeInfo(
      std::string_view host,
      std::string_view realm = "") {
    net::AuthChallengeInfo auth_info;
    auth_info.is_proxy = true;
    auth_info.challenger = url::SchemeHostPort("https", std::string(host), 443);
    if (!realm.empty()) {
      auth_info.realm = std::string(realm);
    }
    return auth_info;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<EnterpriseNetworkAuthService> auth_service_;
  std::unique_ptr<EnterpriseProxyService> proxy_service_;
  std::unique_ptr<EnterpriseProxyErrorService> error_service_;
};

TEST_F(EnterpriseProxyErrorServiceTest, NotApplicableWhenNoManagedProxy) {
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("unmanaged.example.com"),
      GURL("https://target.securegateway.com/test"), nullptr,
      future.GetCallback());

  EXPECT_FALSE(handled);
  EXPECT_FALSE(future.IsReady());
}

TEST_F(EnterpriseProxyErrorServiceTest, DisguisedErrorRealm403_CancelsAuth) {
  SetupManagedDomainWithProxy("proxy.securegateway.com");

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.securegateway.com", "403"),
      GURL("https://target.securegateway.com/test"), nullptr,
      future.GetCallback());

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EnterpriseProxyErrorServiceTest, NoCredentialsNeeded_ReturnsNullopt) {
  SetupManagedDomainWithProxy("proxy.securegateway.com", /*with_auth=*/false);

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.securegateway.com"),
      GURL("https://target.securegateway.com/test"), nullptr,
      future.GetCallback());

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EnterpriseProxyErrorServiceTest, ValidAuthChallenge_FetchesCredentials) {
  SetupManagedDomainWithProxy("proxy.securegateway.com");

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.securegateway.com", "Secure Gateway"),
      GURL("https://target.securegateway.com/test"), nullptr,
      future.GetCallback());

  EXPECT_TRUE(handled);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(u"access_token", future.Get()->password());
}

TEST_F(EnterpriseProxyErrorServiceTest, CredentialFetchFailure_ReturnsNullopt) {
  SetupManagedDomainWithProxy("proxy.securegateway.com");
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.securegateway.com", "Secure Gateway"),
      GURL("https://target.securegateway.com/test"), nullptr,
      future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace enterprise_net
