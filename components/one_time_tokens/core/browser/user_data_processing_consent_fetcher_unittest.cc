// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/user_data_processing_consent_fetcher.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/one_time_tokens/core/browser/fetch_user_data_processing_consent_response.pb.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class UserDataProcessingConsentFetcherTest : public testing::Test {
 public:
  UserDataProcessingConsentFetcherTest()
      : fetcher_(test_url_loader_factory_.GetSafeWeakWrapper(),
                 *identity_test_env_.identity_manager()) {}

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  UserDataProcessingConsentFetcher fetcher_;
};

TEST_F(UserDataProcessingConsentFetcherTest, FetchSuccess) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  fetcher_.Start(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ::google::internal::chrome::passwords::onetimetoken::v1::
      FetchUserDataProcessingConsentResponse response;
  response.set_comms_apps(::google::internal::chrome::passwords::onetimetoken::
                              v1::USER_DATA_PROCESSING_CONSENT_STATE_ENABLED);
  response.set_google_apps(::google::internal::chrome::passwords::onetimetoken::
                               v1::USER_DATA_PROCESSING_CONSENT_STATE_DISABLED);

  test_url_loader_factory_.AddResponse(
      "https://onetimetoken.pa.googleapis.com/v1/"
      "onetimetokens:fetchUserDataProcessingConsent?alt=proto",
      response.SerializeAsString());

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->comms_apps, ConsentState::kEnabled);
  EXPECT_EQ(result->google_apps, ConsentState::kDisabled);
}

TEST_F(UserDataProcessingConsentFetcherTest, FetchNetworkError) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  fetcher_.Start(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  test_url_loader_factory_.AddResponse(
      "https://onetimetoken.pa.googleapis.com/v1/"
      "onetimetokens:fetchUserDataProcessingConsent?alt=proto",
      "", net::HTTP_INTERNAL_SERVER_ERROR);

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

TEST_F(UserDataProcessingConsentFetcherTest, FetchAccessTokenError) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  fetcher_.Start(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

TEST_F(UserDataProcessingConsentFetcherTest, FetchTimeout) {
  base::test::TestFuture<std::optional<UserDataProcessingConsentStates>> future;
  fetcher_.Start(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  task_environment_.FastForwardBy(base::Seconds(3));

  std::optional<UserDataProcessingConsentStates> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

}  // namespace one_time_tokens
