// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/ubertoken_fetcher_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAccountId[] = "test@gmail.com";

class MockUbertokenConsumer {
 public:
  MockUbertokenConsumer()
      : nb_correct_token_(0),
        last_error_(GoogleServiceAuthError::AuthErrorNone()),
        nb_error_(0) {}
  virtual ~MockUbertokenConsumer() = default;

  void OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                const std::string& token) {
    if (error != GoogleServiceAuthError::AuthErrorNone()) {
      last_error_ = error;
      ++nb_error_;
      return;
    }

    last_token_ = token;
    ++nb_correct_token_;
  }

  std::string last_token_;
  int nb_correct_token_;
  GoogleServiceAuthError last_error_;
  int nb_error_;
};

}  // namespace

class UbertokenFetcherImplTest : public testing::Test {
 public:
  UbertokenFetcherImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        token_service_(&pref_service_),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {
    fetcher_ = std::make_unique<signin::UbertokenFetcherImpl>(
        CoreAccountId(kTestAccountId), &token_service_,
        base::BindOnce(&MockUbertokenConsumer::OnUbertokenFetchComplete,
                       base::Unretained(&consumer_)),
        gaia::GaiaSource::kChrome, test_shared_loader_factory_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  FakeProfileOAuth2TokenService token_service_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  MockUbertokenConsumer consumer_;
  std::unique_ptr<signin::UbertokenFetcherImpl> fetcher_;
};

TEST_F(UbertokenFetcherImplTest, Basic) {}

TEST_F(UbertokenFetcherImplTest, Success) {
  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));
  fetcher_->OnUberAuthTokenSuccess("uberToken");

  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}

TEST_F(UbertokenFetcherImplTest, NoRefreshToken) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  fetcher_->OnGetTokenFailure(nullptr, error);

  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
}

TEST_F(UbertokenFetcherImplTest, FailureToGetAccessToken) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  fetcher_->OnGetTokenFailure(nullptr, error);

  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);
}

TEST_F(UbertokenFetcherImplTest, TransientFailureEventualFailure) {
  GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));

  for (int i = 0; i < signin::UbertokenFetcherImpl::kMaxRetries; ++i) {
    fetcher_->OnUberAuthTokenFailure(error);
    EXPECT_EQ(0, consumer_.nb_error_);
    EXPECT_EQ(0, consumer_.nb_correct_token_);
    EXPECT_EQ("", consumer_.last_token_);
  }

  fetcher_->OnUberAuthTokenFailure(error);
  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);
}

TEST_F(UbertokenFetcherImplTest, TransientFailureEventualSuccess) {
  GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));

  for (int i = 0; i < signin::UbertokenFetcherImpl::kMaxRetries; ++i) {
    fetcher_->OnUberAuthTokenFailure(error);
    EXPECT_EQ(0, consumer_.nb_error_);
    EXPECT_EQ(0, consumer_.nb_correct_token_);
    EXPECT_EQ("", consumer_.last_token_);
  }

  fetcher_->OnUberAuthTokenSuccess("uberToken");
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}

TEST_F(UbertokenFetcherImplTest, PermanentFailureEventualFailure) {
  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));

  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  fetcher_->OnUberAuthTokenFailure(error);
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);

  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));
  fetcher_->OnUberAuthTokenFailure(error);
  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);
}

TEST_F(UbertokenFetcherImplTest, PermanentFailureEventualSuccess) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));

  fetcher_->OnUberAuthTokenFailure(error);
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);

  fetcher_->OnGetTokenSuccess(nullptr,
                              OAuth2AccessTokenConsumer::TokenResponse(
                                  "accessToken", base::Time(), std::string()));
  fetcher_->OnUberAuthTokenSuccess("uberToken");
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}
