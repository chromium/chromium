// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/auth.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
          "domains": ["*.example.com"]
        }
      ]
    })";

class TestDelegate : public EnterpriseProxyErrorService::Delegate {
 public:
  explicit TestDelegate(bool* attached_flag = nullptr,
                        EnterpriseProxyErrorData* error_data_out = nullptr,
                        bool* signin_prompt_shown_out = nullptr,
                        GURL* signin_destination_url_out = nullptr)
      : attached_flag_(attached_flag),
        error_data_out_(error_data_out),
        signin_prompt_shown_out_(signin_prompt_shown_out),
        signin_destination_url_out_(signin_destination_url_out) {}
  ~TestDelegate() override = default;

  const EnterpriseProxyErrorData* GetDisguisedErrorData() const override {
    return has_error_data_ ? &error_data_ : nullptr;
  }

  void AttachDisguisedErrorData(
      const EnterpriseProxyErrorData& error_data) override {
    has_error_data_ = true;
    error_data_ = error_data;
    if (attached_flag_) {
      *attached_flag_ = true;
    }
    if (error_data_out_) {
      *error_data_out_ = error_data;
    }
  }

  void OnSignInRequired(const GURL& destination_url) override {
    if (signin_prompt_shown_out_) {
      *signin_prompt_shown_out_ = true;
    }
    if (signin_destination_url_out_) {
      *signin_destination_url_out_ = destination_url;
    }
  }

 private:
  raw_ptr<bool> attached_flag_ = nullptr;
  raw_ptr<EnterpriseProxyErrorData> error_data_out_ = nullptr;
  raw_ptr<bool> signin_prompt_shown_out_ = nullptr;
  raw_ptr<GURL> signin_destination_url_out_ = nullptr;
  bool has_error_data_ = false;
  EnterpriseProxyErrorData error_data_;
};

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
    feature_list_.InitWithFeatures(
        {kEnableDynamicRouteFetching, kEnterpriseProxyErrorHandling}, {});
  }

  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());

    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "user@managed.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.GetAccountId(), account_info.GetEmail(),
        account_info.GetGaiaId(), "managed.com", "Full Name", "Given Name",
        "en-US", "picture_url");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();

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
    test_url_loader_factory_.reset();
  }

  void SetupManagedDomainWithProxy(std::string_view proxy_host,
                                   bool with_auth = true) {
    base::ListValue policy_domains;
    policy_domains.Append(CreateDomainPolicyEntry("example-pvd.com", true));
    pref_service_.SetList(kProxyProvisioningDomains, std::move(policy_domains));

    std::string auth_block = with_auth ? kAuthBlockJson : "";
    std::string pvd_config_json =
        base::StringPrintf(kPvdConfigJsonTemplate,
                           std::string(proxy_host).c_str(), auth_block.c_str());

    test_url_loader_factory_->SimulateResponseForPendingRequest(
        "https://example-pvd.com/.well-known/pvd", pvd_config_json);
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
  std::unique_ptr<EnterpriseNetworkAuthService> auth_service_;
  std::unique_ptr<EnterpriseProxyService> proxy_service_;
  std::unique_ptr<EnterpriseProxyErrorService> error_service_;
};

TEST_F(EnterpriseProxyErrorServiceTest, NotApplicableWhenNoManagedProxy) {
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("unmanaged.example.com"),
      GURL("https://target.example.com/test"), nullptr,
      /*delegate=*/nullptr, future.GetCallback());

  EXPECT_FALSE(handled);
  EXPECT_FALSE(future.IsReady());
}

TEST_F(EnterpriseProxyErrorServiceTest, DisguisedErrorRealm403_CancelsAuth) {
  SetupManagedDomainWithProxy("proxy.example.com");

  bool attached = false;
  EnterpriseProxyErrorData attached_data;
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "403"),
      GURL("https://target.example.com/test"), nullptr,
      std::make_unique<TestDelegate>(&attached, &attached_data),
      future.GetCallback());

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_TRUE(attached);
  EXPECT_EQ(attached_data.destination_url(),
            GURL("https://target.example.com/test"));
  EXPECT_EQ(attached_data.proxy_url(), GURL("https://proxy.example.com:443"));
  EXPECT_EQ(attached_data.error_code(), 403);
}

TEST_F(EnterpriseProxyErrorServiceTest,
       DisguisedErrorOtherRealms_CancelsAuthAndAttachesData) {
  for (int error_code : {500, 502, 503, 504}) {
    SetupManagedDomainWithProxy("proxy.example.com");

    bool attached = false;
    EnterpriseProxyErrorData attached_data;
    base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
    bool handled = error_service_->InterceptProxyAuthChallenge(
        CreateProxyAuthChallengeInfo("proxy.example.com",
                                     base::NumberToString(error_code)),
        GURL("https://target.example.com/test"), nullptr,
        std::make_unique<TestDelegate>(&attached, &attached_data),
        future.GetCallback());

    EXPECT_TRUE(handled);
    EXPECT_FALSE(future.Get().has_value());
    EXPECT_TRUE(attached);
    EXPECT_EQ(attached_data.error_code(), error_code);
  }
}

TEST_F(EnterpriseProxyErrorServiceTest,
       InvalidOrUnsupportedRealm_ProceedsAsStandardAuth) {
  SetupManagedDomainWithProxy("proxy.example.com");

  bool attached = false;
  EnterpriseProxyErrorData attached_data;
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  // A realm like "404" or "unknown" is not a disguised error code, so it is
  // treated as a standard proxy auth challenge.
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "404"),
      GURL("https://target.example.com/test"), nullptr,
      std::make_unique<TestDelegate>(&attached, &attached_data),
      future.GetCallback());

  EXPECT_TRUE(handled);
  // It fetches standard OAuth credentials rather than canceling as a disguised
  // error.
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(u"access_token", future.Get()->password());
  EXPECT_FALSE(attached);
}

TEST_F(EnterpriseProxyErrorServiceTest,
       ForcedDisguisedErrorCodeParam_ForcesError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnterpriseProxyErrorHandling, {{"forced_disguised_error_code", "503"}});

  SetupManagedDomainWithProxy("proxy.example.com");

  bool attached = false;
  EnterpriseProxyErrorData attached_data;
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  // Even with a normal realm like "Enterprise Realm", the forced param
  // overrides it to 503.
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "Enterprise Realm"),
      GURL("https://target.example.com/test"), nullptr,
      std::make_unique<TestDelegate>(&attached, &attached_data),
      future.GetCallback());

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_TRUE(attached);
  EXPECT_EQ(attached_data.error_code(), 503);
}

TEST_F(EnterpriseProxyErrorServiceTest, NoCredentialsNeeded_ReturnsNullopt) {
  SetupManagedDomainWithProxy("proxy.example.com", /*with_auth=*/false);

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com"),
      GURL("https://target.example.com/test"), nullptr,
      /*delegate=*/nullptr, future.GetCallback());

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EnterpriseProxyErrorServiceTest, ValidAuthChallenge_FetchesCredentials) {
  SetupManagedDomainWithProxy("proxy.example.com");

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "Enterprise Realm"),
      GURL("https://target.example.com/test"), nullptr,
      /*delegate=*/nullptr, future.GetCallback());

  EXPECT_TRUE(handled);
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(u"access_token", future.Get()->password());
}

TEST_F(EnterpriseProxyErrorServiceTest, CredentialFetchFailure_ReturnsNullopt) {
  SetupManagedDomainWithProxy("proxy.example.com");
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "Enterprise Realm"),
      GURL("https://target.example.com/test"), nullptr,
      /*delegate=*/nullptr, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceUnavailable("error"));

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(EnterpriseProxyErrorServiceTest,
       SignInRequired_InvokesDelegateOnSignInRequired) {
  SetupManagedDomainWithProxy("proxy.example.com");
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  bool signin_prompt_shown = false;
  GURL signin_destination_url;
  base::test::TestFuture<const std::optional<net::AuthCredentials>&> future;
  bool handled = error_service_->InterceptProxyAuthChallenge(
      CreateProxyAuthChallengeInfo("proxy.example.com", "Enterprise Realm"),
      GURL("https://target.example.com/test"), nullptr,
      std::make_unique<TestDelegate>(/*attached_flag=*/nullptr,
                                     /*error_data_out=*/nullptr,
                                     &signin_prompt_shown,
                                     &signin_destination_url),
      future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_TRUE(handled);
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_TRUE(signin_prompt_shown);
  EXPECT_EQ(signin_destination_url, GURL("https://target.example.com/test"));
}

TEST_F(EnterpriseProxyErrorServiceTest, GetErrorPageHTML_NullDelegate) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(error_service_->GetErrorPageHTML(nullptr).empty());
  histogram_tester.ExpectTotalCount(
      "Enterprise.Proxy.DisguisedErrorPage.ErrorCode", 0);
}

TEST_F(EnterpriseProxyErrorServiceTest, GetErrorPageHTML_NoDisguisedErrorData) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  EXPECT_TRUE(error_service_->GetErrorPageHTML(&delegate).empty());
  histogram_tester.ExpectTotalCount(
      "Enterprise.Proxy.DisguisedErrorPage.ErrorCode", 0);
}

TEST_F(EnterpriseProxyErrorServiceTest, GetErrorPageHTML_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnterpriseProxyErrorHandling);

  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  EnterpriseProxyErrorData data(GURL("https://target.example.com/page"),
                                GURL("https://proxy.example.com:443"), 403);
  delegate.AttachDisguisedErrorData(data);

  EXPECT_TRUE(error_service_->GetErrorPageHTML(&delegate).empty());
  histogram_tester.ExpectTotalCount(
      "Enterprise.Proxy.DisguisedErrorPage.ErrorCode", 0);
}

TEST_F(EnterpriseProxyErrorServiceTest, GetErrorPageHTML_ValidData) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  EnterpriseProxyErrorData data(GURL("https://target.example.com/page"),
                                GURL("https://proxy.example.com:443"), 403);
  delegate.AttachDisguisedErrorData(data);

  std::string html = error_service_->GetErrorPageHTML(&delegate);
  EXPECT_FALSE(html.empty());
  EXPECT_NE(html.find("https://target.example.com/page"), std::string::npos);
  EXPECT_NE(html.find("https://proxy.example.com/"), std::string::npos);
  EXPECT_NE(html.find("403"), std::string::npos);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.Proxy.DisguisedErrorPage.ErrorCode", 403, 1);
}

}  // namespace enterprise_net
