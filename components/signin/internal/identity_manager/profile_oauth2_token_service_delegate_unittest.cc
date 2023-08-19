// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockOAuth2TokenServiceObserver
    : public ProfileOAuth2TokenServiceObserver {
 public:
  MOCK_METHOD2(OnAuthErrorChanged,
               void(const CoreAccountId&, const GoogleServiceAuthError&));
};

class ProfileOAuth2TokenServiceDelegateTest : public testing::Test {
 public:
  ProfileOAuth2TokenServiceDelegateTest() { delegate_.AddObserver(&observer_); }

  ~ProfileOAuth2TokenServiceDelegateTest() override {
    delegate_.RemoveObserver(&observer_);
  }

  // Note: Tests in this suite tests the default base implementation of
  //       'ProfileOAuth2TokenServiceDelegate'.
  FakeProfileOAuth2TokenServiceDelegate delegate_;
  MockOAuth2TokenServiceObserver observer_;
};

// The default implementation of
// ProfileOAuth2TokenServiceDelegate::InvalidateTokensForMultilogin is used on
// mobile where refresh tokens are not accessible. This test checks that refresh
// tokens are not affected on INVALID_TOKENS Multilogin error.
TEST_F(ProfileOAuth2TokenServiceDelegateTest, InvalidateTokensForMultilogin) {
  // Check that OnAuthErrorChanged is not fired from
  // InvalidateTokensForMultilogin and refresh tokens are not set in error.
  EXPECT_CALL(
      observer_,
      OnAuthErrorChanged(::testing::_,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(0);

  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  delegate_.UpdateCredentials(account_id1, "refresh_token1");
  delegate_.UpdateCredentials(account_id2, "refresh_token2");

  delegate_.InvalidateTokenForMultilogin(account_id1);

  EXPECT_EQ(delegate_.GetAuthError(account_id1).state(),
            GoogleServiceAuthError::NONE);
  EXPECT_EQ(delegate_.GetAuthError(account_id2).state(),
            GoogleServiceAuthError::NONE);
}

// Contains all non-deprecated Google service auth error states.
const GoogleServiceAuthError::State table[] = {
    GoogleServiceAuthError::NONE,
    GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
    GoogleServiceAuthError::USER_NOT_SIGNED_UP,
    GoogleServiceAuthError::CONNECTION_FAILED,
    GoogleServiceAuthError::SERVICE_UNAVAILABLE,
    GoogleServiceAuthError::REQUEST_CANCELED,
    GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
    GoogleServiceAuthError::SERVICE_ERROR,
    GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR,
    GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED,
};

TEST_F(ProfileOAuth2TokenServiceDelegateTest, UpdateAuthError_PersistenErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate_.UpdateCredentials(account_id, "refresh_token");

  static_assert(
      std::size(table) == GoogleServiceAuthError::NUM_STATES -
                              GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");

  for (GoogleServiceAuthError::State state : table) {
    GoogleServiceAuthError error(state);
    if (!error.IsPersistentError() || error.IsScopePersistentError())
      continue;

    EXPECT_CALL(observer_, OnAuthErrorChanged(account_id, error)).Times(1);

    delegate_.UpdateAuthError(account_id, error);
    EXPECT_EQ(delegate_.GetAuthError(account_id), error);
    // Backoff only used for transient errors.
    EXPECT_EQ(delegate_.BackoffEntry()->failure_count(), 0);
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest, UpdateAuthError_TransientErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate_.UpdateCredentials(account_id, "refresh_token");

  static_assert(
      std::size(table) == GoogleServiceAuthError::NUM_STATES -
                              GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");

  EXPECT_TRUE(delegate_.BackoffEntry());
  int failure_count = 0;
  for (GoogleServiceAuthError::State state : table) {
    GoogleServiceAuthError error(state);
    if (!error.IsTransientError())
      continue;

    EXPECT_CALL(observer_, OnAuthErrorChanged(account_id, ::testing::_))
        .Times(0);

    failure_count++;
    delegate_.UpdateAuthError(account_id, error);
    EXPECT_EQ(delegate_.GetAuthError(account_id),
              GoogleServiceAuthError::AuthErrorNone());
    EXPECT_GT(delegate_.BackoffEntry()->GetTimeUntilRelease(),
              base::TimeDelta());
    EXPECT_EQ(delegate_.BackoffEntry()->failure_count(), failure_count);
    EXPECT_EQ(delegate_.BackOffError(), error);
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest,
       UpdateAuthError_ScopePersistenErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate_.UpdateCredentials(account_id, "refresh_token");
  GoogleServiceAuthError error(
      GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR);

  // Scope persistent errors are not persisted or notified as it does not imply
  // that the account is in an error state but the error is only relevant to
  // the scope set requested in the access token request.
  EXPECT_CALL(observer_, OnAuthErrorChanged(account_id, error)).Times(0);

  delegate_.UpdateAuthError(account_id, error);
  EXPECT_EQ(delegate_.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());
  // Backoff only used for transient errors.
  EXPECT_EQ(delegate_.BackoffEntry()->failure_count(), 0);
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest,
       UpdateAuthError_RefreshTokenNotAvailable) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  EXPECT_FALSE(delegate_.RefreshTokenIsAvailable(account_id));
  EXPECT_CALL(observer_, OnAuthErrorChanged(::testing::_, ::testing::_))
      .Times(0);
  delegate_.UpdateAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(delegate_.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());
  testing::Mock::VerifyAndClearExpectations(&observer_);
  // Backoff only used for transient errors.
  EXPECT_EQ(delegate_.BackoffEntry()->failure_count(), 0);
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest, AuthErrorChanged) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate_.UpdateCredentials(account_id, "refresh_token");

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(observer_, OnAuthErrorChanged(account_id, error)).Times(1);
  delegate_.UpdateAuthError(account_id, error);
  EXPECT_EQ(delegate_.GetAuthError(account_id), error);

  // Update the error but without changing it should not fire a notification.
  delegate_.UpdateAuthError(account_id, error);
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Change the error.
  EXPECT_CALL(
      observer_,
      OnAuthErrorChanged(account_id, GoogleServiceAuthError::AuthErrorNone()))
      .Times(1);
  delegate_.UpdateAuthError(account_id,
                            GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(delegate_.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());

  // Update to none again should not fire any notification.
  delegate_.UpdateAuthError(account_id,
                            GoogleServiceAuthError::AuthErrorNone());
  testing::Mock::VerifyAndClearExpectations(&observer_);
}
