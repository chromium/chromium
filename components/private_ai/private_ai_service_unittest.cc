// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/private_ai_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/mock_blind_sign_auth.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/testing/fake_private_ai_network_driver.h"
#include "components/private_ai/testing/fake_private_ai_oak_session_driver.h"
#include "components/private_ai/testing/test_blind_sign_auth_factory.h"
#include "components/private_ai/testing/test_private_ai_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

quiche::BlindSignToken CreateBlindSignTokenForTesting(std::string token_value,
                                                      base::Time expiration) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  std::string encoded_token_value = base::Base64Encode(token_value);
  std::string encoded_extension_value =
      base::Base64Encode("mock-extension-value");

  privacy_pass_token_data.set_token(std::move(encoded_token_value));
  privacy_pass_token_data.set_encoded_extensions(
      std::move(encoded_extension_value));

  quiche::BlindSignToken blind_sign_token;
  blind_sign_token.token = privacy_pass_token_data.SerializeAsString();
  blind_sign_token.expiration = absl::FromTimeT(expiration.ToTimeT());

  return blind_sign_token;
}

constexpr char kTestEmail[] = "test@example.com";

}  // namespace

class PrivateAiServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});
    auto test_bsa_factory = std::make_unique<TestBlindSignAuthFactory>();

    auto shared_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    private_ai_service_ = std::make_unique<TestPrivateAiService>(
        identity_test_env_.identity_manager(),
        std::move(shared_url_loader_factory), &test_network_context_,
        "dummy.com", PrivateAiService::GetApiKey(version_info::Channel::STABLE),
        "dummy-proxy.com",
        /*use_token_attestation=*/false,
        std::make_unique<FakePrivateAiNetworkDriver>(),
        std::make_unique<FakePrivateAiOakSessionDriver>(),
        std::move(test_bsa_factory), version_info::Channel::STABLE);
  }

  void TearDown() override {
    private_ai_service_->Shutdown();
    private_ai_service_.reset();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  network::TestNetworkContext test_network_context_;

 private:
  base::test::ScopedFeatureList feature_list_;

 protected:
  std::unique_ptr<TestPrivateAiService> private_ai_service_;
};

TEST_F(PrivateAiServiceTest, RequestOAuthTokenSuccess) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kSuccess);
  EXPECT_EQ(future.Get<1>(), "access_token");
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenNoAccount) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  private_ai_service_->RequestOAuthToken(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kFailedNoAccount);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenTransientError) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  EXPECT_EQ(future.Get<0>(),
            phosphor::GetAuthnTokensResult::kFailedOAuthTokenTransient);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenPersistentError) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EXPECT_EQ(future.Get<0>(),
            phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

class PrivateAiServiceUtilTest : public testing::Test {};

TEST_F(PrivateAiServiceUtilTest, GetApiKey) {
  // If API key is set in feature params, it should be returned.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPrivateAi, {{"api-key", "provided-api-key"}}}}, {});
    EXPECT_EQ(PrivateAiService::GetApiKey(version_info::Channel::STABLE),
              "provided-api-key");
  }

  // If API key is not set, it should return default Chrome API key (if used) or
  // empty. This depends on the build configuration.
  {
    if (google_apis::IsGoogleChromeAPIKeyUsed()) {
      EXPECT_EQ(PrivateAiService::GetApiKey(version_info::Channel::STABLE),
                google_apis::GetAPIKey(version_info::Channel::STABLE));
    } else {
      EXPECT_EQ(PrivateAiService::GetApiKey(version_info::Channel::STABLE), "");
    }
  }
}

TEST_F(PrivateAiServiceUtilTest, CanPrivateAiBeEnabled) {
  // Enabled by default in features.cc, but needs API key.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPrivateAi, {{"api-key", "test-key"}}}}, {});
    EXPECT_TRUE(
        PrivateAiService::CanPrivateAiBeEnabled(version_info::Channel::STABLE));
  }

  // Disabled if API key is empty AND not using default key.
  {
    // If google_apis::IsGoogleChromeAPIKeyUsed() is true, it will still be
    // enabled because GetApiKey() will return the default key. We need to
    // account for that.
    if (google_apis::IsGoogleChromeAPIKeyUsed()) {
      EXPECT_TRUE(PrivateAiService::CanPrivateAiBeEnabled(
          version_info::Channel::STABLE));
    } else {
      EXPECT_FALSE(PrivateAiService::CanPrivateAiBeEnabled(
          version_info::Channel::STABLE));
    }
  }
}

TEST_F(PrivateAiServiceTest,
       OnErrorStateOfRefreshTokenUpdatedForAccount_ResetsBackoff) {
  // Set some mock BSA tokens. They will only be returned after oauth tokens
  // have been fetched.
  private_ai_service_->mock_bsa()->set_tokens({CreateBlindSignTokenForTesting(
      "mock-token-value", base::Time::Now() + base::Hours(1))});

  // Set up primary account.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Trigger a persistent OAuth token fetch failure, and update the persistent
  // error of the refresh token in the IdentityManager to a persistent error
  // state.
  {
    base::test::TestFuture<phosphor::GetAuthnTokensResult,
                           std::optional<std::string>>
        future;
    private_ai_service_->RequestOAuthToken(future.GetCallback());
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
    identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
        account_info.GetAccountId(),
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
    EXPECT_EQ(future.Get<0>(),
              phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent);
  }

  // Calling GetAuthToken on the token manager should immediately fail with
  // nullopt due to the Max backoff.
  {
    base::test::TestFuture<std::optional<phosphor::BlindSignedAuthToken>>
        future;
    private_ai_service_->GetTokenManager()->GetAuthToken(future.GetCallback());
    EXPECT_EQ(future.Get(), std::nullopt);
  }

  // Fix the sign-in state of the account (the refresh token error transitions
  // to NONE).
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // The backoff should be reset. Calling GetAuthToken should now trigger
  // a new OAuth token request instead of failing immediately.
  {
    base::test::TestFuture<std::optional<phosphor::BlindSignedAuthToken>>
        future;
    private_ai_service_->GetTokenManager()->GetAuthToken(future.GetCallback());

    // A new OAuth token request should be triggered.
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "access_token", base::Time::Now() + base::Hours(1));

    // Wait for GetAuthToken to complete. It should successfully return the
    // valid mock token.
    std::optional<phosphor::BlindSignedAuthToken> result = future.Get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->token, "bW9jay10b2tlbi12YWx1ZQ==");
  }
}

}  // namespace private_ai
