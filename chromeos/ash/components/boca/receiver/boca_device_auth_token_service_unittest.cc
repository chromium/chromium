// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/boca_device_auth_token_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

using testing::_;
using testing::NotNull;
using testing::Return;
using testing::StrEq;

constexpr std::string_view kAccessToken = "test_token";

class MockDeviceOAuth2TokenService {
 public:
  MockDeviceOAuth2TokenService() {}
  ~MockDeviceOAuth2TokenService() = default;

  MOCK_METHOD(std::unique_ptr<OAuth2AccessTokenManager::Request>,
              StartAccessTokenRequest,
              (const OAuth2AccessTokenManager::ScopeSet&,
               OAuth2AccessTokenManager::Consumer*));
  MOCK_METHOD(bool, RefreshTokenIsAvailable, (), (const));
  MOCK_METHOD(void,
              InvalidateAccessToken,
              (const OAuth2AccessTokenManager::ScopeSet&, const std::string&));
};

class MockDeviceOAuth2TokenServiceFactory {
 public:
  static MockDeviceOAuth2TokenService* Get() { return instance_; }
  static void SetInstance(MockDeviceOAuth2TokenService* instance) {
    instance_ = instance;
  }

 private:
  static MockDeviceOAuth2TokenService* instance_;
};

MockDeviceOAuth2TokenService* MockDeviceOAuth2TokenServiceFactory::instance_ =
    nullptr;

class BocaDeviceAuthTokenServiceTest : public testing::Test {
 public:
  BocaDeviceAuthTokenServiceTest() : scopes_({"scope1", "scope2"}) {}

 protected:
  void SetUp() override {
    mock_token_service_ = std::make_unique<MockDeviceOAuth2TokenService>();
    MockDeviceOAuth2TokenServiceFactory::SetInstance(mock_token_service_.get());
    test_clock_.SetNow(base::Time::Now());
    service_ = std::make_unique<
        BocaDeviceAuthTokenService<MockDeviceOAuth2TokenServiceFactory>>(
        scopes_, "requester_id", &test_clock_);
  }

  void TearDown() override {
    MockDeviceOAuth2TokenServiceFactory::SetInstance(nullptr);
  }

  base::SimpleTestClock test_clock_;
  const OAuth2AccessTokenManager::ScopeSet scopes_;
  std::unique_ptr<
      BocaDeviceAuthTokenService<MockDeviceOAuth2TokenServiceFactory>>
      service_;
  std::unique_ptr<MockDeviceOAuth2TokenService> mock_token_service_;
};

TEST_F(BocaDeviceAuthTokenServiceTest, StartAuthenticationSuccess) {
  std::optional<google_apis::ApiErrorCode> error_code;
  std::string access_token;
  EXPECT_CALL(*mock_token_service_, StartAccessTokenRequest(scopes_, _))
      .WillOnce([](const OAuth2AccessTokenManager::ScopeSet&,
                   OAuth2AccessTokenManager::Consumer* consumer)
                    -> std::unique_ptr<OAuth2AccessTokenManager::Request> {
        consumer->OnGetTokenSuccess(
            /*request=*/nullptr,
            OAuth2AccessTokenConsumer::TokenResponse::Builder()
                .WithAccessToken(std::string(kAccessToken))
                .WithExpirationTime(base::Time::Now() + base::Hours(1))
                .build());
        return nullptr;
      });

  service_->StartAuthentication(base::BindLambdaForTesting(
      [&error_code, &access_token](google_apis::ApiErrorCode code,
                                   const std::string& token) {
        error_code = code;
        access_token = token;
      }));

  ASSERT_TRUE(error_code.has_value());
  EXPECT_EQ(error_code.value(), google_apis::HTTP_SUCCESS);
  EXPECT_THAT(access_token, StrEq(kAccessToken));
  EXPECT_TRUE(service_->HasAccessToken());
  EXPECT_THAT(service_->access_token(), StrEq(kAccessToken));

  EXPECT_CALL(*mock_token_service_,
              InvalidateAccessToken(scopes_, std::string(kAccessToken)));
  service_->ClearAccessToken();
  EXPECT_FALSE(service_->HasAccessToken());
  EXPECT_TRUE(service_->access_token().empty());
}

TEST_F(BocaDeviceAuthTokenServiceTest, StartAuthenticationConnectionFailure) {
  std::optional<google_apis::ApiErrorCode> error_code;
  std::string access_token;
  EXPECT_CALL(*mock_token_service_, StartAccessTokenRequest(scopes_, _))
      .WillOnce([](const OAuth2AccessTokenManager::ScopeSet&,
                   OAuth2AccessTokenManager::Consumer* consumer)
                    -> std::unique_ptr<OAuth2AccessTokenManager::Request> {
        consumer->OnGetTokenFailure(
            /*request=*/nullptr,
            GoogleServiceAuthError(
                GoogleServiceAuthError::State::CONNECTION_FAILED));
        return nullptr;
      });
  service_->StartAuthentication(base::BindLambdaForTesting(
      [&error_code, &access_token](google_apis::ApiErrorCode code,
                                   const std::string& token) {
        error_code = code;
        access_token = token;
      }));

  ASSERT_TRUE(error_code.has_value());
  EXPECT_EQ(error_code.value(), google_apis::NO_CONNECTION);
  EXPECT_TRUE(access_token.empty());
  EXPECT_FALSE(service_->HasAccessToken());
}

TEST_F(BocaDeviceAuthTokenServiceTest, StartAuthenticationAuthFailure) {
  std::optional<google_apis::ApiErrorCode> error_code;
  std::string access_token;
  EXPECT_CALL(*mock_token_service_, StartAccessTokenRequest(scopes_, _))
      .WillOnce([](const OAuth2AccessTokenManager::ScopeSet&,
                   OAuth2AccessTokenManager::Consumer* consumer)
                    -> std::unique_ptr<OAuth2AccessTokenManager::Request> {
        consumer->OnGetTokenFailure(
            /*request=*/nullptr,
            GoogleServiceAuthError(
                GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
        return nullptr;
      });
  service_->StartAuthentication(base::BindLambdaForTesting(
      [&error_code, &access_token](google_apis::ApiErrorCode code,
                                   const std::string& token) {
        error_code = code;
        access_token = token;
      }));

  ASSERT_TRUE(error_code.has_value());
  EXPECT_EQ(error_code.value(), google_apis::HTTP_UNAUTHORIZED);
  EXPECT_TRUE(access_token.empty());
  EXPECT_FALSE(service_->HasAccessToken());
}

TEST_F(BocaDeviceAuthTokenServiceTest, HasRefreshToken) {
  EXPECT_CALL(*mock_token_service_, RefreshTokenIsAvailable())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_FALSE(service_->HasRefreshToken());
  EXPECT_TRUE(service_->HasRefreshToken());
}

TEST_F(BocaDeviceAuthTokenServiceTest, AccessTokenExpired) {
  EXPECT_CALL(*mock_token_service_, StartAccessTokenRequest(scopes_, _))
      .WillOnce([](const OAuth2AccessTokenManager::ScopeSet&,
                   OAuth2AccessTokenManager::Consumer* consumer)
                    -> std::unique_ptr<OAuth2AccessTokenManager::Request> {
        consumer->OnGetTokenSuccess(
            /*request=*/nullptr,
            OAuth2AccessTokenConsumer::TokenResponse::Builder()
                .WithAccessToken(std::string(kAccessToken))
                .WithExpirationTime(base::Time::Now() + base::Hours(1))
                .build());
        return nullptr;
      });

  service_->StartAuthentication(base::DoNothing());

  EXPECT_TRUE(service_->HasAccessToken());
  test_clock_.SetNow(base::Time::Now() + base::Hours(2));
  EXPECT_FALSE(service_->HasAccessToken());
  EXPECT_TRUE(service_->access_token().empty());
}

}  // namespace
}  // namespace ash::boca_receiver
