// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

#include <stddef.h>

#include <string>

#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileOAuth2TokenServiceDelegateTest : public testing::Test {
 public:
  ProfileOAuth2TokenServiceDelegateTest() {
    delegate.SetOnRefreshTokenRevokedNotified(
        on_refresh_token_revoked_notified_callback.Get());
  }

  ~ProfileOAuth2TokenServiceDelegateTest() override = default;

  base::MockRepeatingCallback<void(const CoreAccountId&)>
      on_refresh_token_revoked_notified_callback;
  // Note: Tests in this suite tests the default base implementation of
  //       'ProfileOAuth2TokenServiceDelegate'.
  FakeProfileOAuth2TokenServiceDelegate delegate;
  signin::MockProfileOAuth2TokenServiceObserver mock_observer{&delegate};
};

TEST_F(ProfileOAuth2TokenServiceDelegateTest, FireRefreshTokenRevoked) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id1");
  delegate.UpdateCredentials(account_id, "refresh_token1");
  testing::InSequence sequence;
  EXPECT_CALL(mock_observer, OnRefreshTokenRevoked(account_id));
  EXPECT_CALL(on_refresh_token_revoked_notified_callback, Run(account_id));
  delegate.FireRefreshTokenRevoked(account_id);
}

// The default implementation of
// ProfileOAuth2TokenServiceDelegate::InvalidateTokensForMultilogin is used on
// mobile where refresh tokens are not accessible. This test checks that refresh
// tokens are not affected on INVALID_TOKENS Multilogin error.
TEST_F(ProfileOAuth2TokenServiceDelegateTest, InvalidateTokensForMultilogin) {
  // Check that OnAuthErrorChanged is not fired from
  // InvalidateTokensForMultilogin and refresh tokens are not set in error.
  EXPECT_CALL(mock_observer,
              OnAuthErrorChanged(
                  ::testing::_,
                  GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                          CREDENTIALS_REJECTED_BY_SERVER),
                  testing::_))
      .Times(0);

  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  delegate.UpdateCredentials(account_id1, "refresh_token1");
  delegate.UpdateCredentials(account_id2, "refresh_token2");

  delegate.InvalidateTokenForMultilogin(account_id1);

  EXPECT_EQ(delegate.GetAuthError(account_id1).state(),
            GoogleServiceAuthError::NONE);
  EXPECT_EQ(delegate.GetAuthError(account_id2).state(),
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

TEST_F(ProfileOAuth2TokenServiceDelegateTest, UpdateAuthErrorPersistenErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate.UpdateCredentials(account_id, "refresh_token");

  static_assert(
      std::size(table) == GoogleServiceAuthError::NUM_STATES -
                              GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");

  for (GoogleServiceAuthError::State state : table) {
    GoogleServiceAuthError error(state);
    if (!error.IsPersistentError() || error.IsScopePersistentError()) {
      continue;
    }

    EXPECT_CALL(mock_observer,
                OnAuthErrorChanged(
                    account_id, error,
                    signin_metrics::SourceForRefreshTokenOperation::kUnknown))
        .Times(1);

    delegate.UpdateAuthError(account_id, error);
    EXPECT_EQ(delegate.GetAuthError(account_id), error);
    // Backoff only used for transient errors.
    EXPECT_EQ(delegate.BackoffEntry()->failure_count(), 0);
    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest, UpdateAuthErrorTransientErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate.UpdateCredentials(account_id, "refresh_token");

  static_assert(
      std::size(table) == GoogleServiceAuthError::NUM_STATES -
                              GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");

  EXPECT_TRUE(delegate.BackoffEntry());
  int failure_count = 0;
  for (GoogleServiceAuthError::State state : table) {
    GoogleServiceAuthError error(state);
    if (!error.IsTransientError()) {
      continue;
    }

    EXPECT_CALL(mock_observer,
                OnAuthErrorChanged(
                    account_id, ::testing::_,
                    signin_metrics::SourceForRefreshTokenOperation::kUnknown))
        .Times(0);

    failure_count++;
    delegate.UpdateAuthError(account_id, error);
    EXPECT_EQ(delegate.GetAuthError(account_id),
              GoogleServiceAuthError::AuthErrorNone());
    EXPECT_GT(delegate.BackoffEntry()->GetTimeUntilRelease(),
              base::TimeDelta());
    EXPECT_EQ(delegate.BackoffEntry()->failure_count(), failure_count);
    EXPECT_EQ(delegate.BackOffError(), error);
    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest,
       UpdateAuthErrorScopePersistenErrors) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate.UpdateCredentials(account_id, "refresh_token");
  GoogleServiceAuthError error(
      GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR);

  // Scope persistent errors are not persisted or notified as it does not imply
  // that the account is in an error state but the error is only relevant to
  // the scope set requested in the access token request.
  EXPECT_CALL(mock_observer, OnAuthErrorChanged(account_id, error, testing::_))
      .Times(0);

  delegate.UpdateAuthError(account_id, error);
  EXPECT_EQ(delegate.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());
  // Backoff only used for transient errors.
  EXPECT_EQ(delegate.BackoffEntry()->failure_count(), 0);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest,
       UpdateAuthErrorRefreshTokenNotAvailable) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  EXPECT_FALSE(delegate.RefreshTokenIsAvailable(account_id));
  EXPECT_CALL(mock_observer,
              OnAuthErrorChanged(::testing::_, ::testing::_, testing::_))
      .Times(0);
  delegate.UpdateAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(delegate.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  // Backoff only used for transient errors.
  EXPECT_EQ(delegate.BackoffEntry()->failure_count(), 0);
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest, AuthErrorChanged) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  delegate.UpdateCredentials(account_id, "refresh_token");

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(mock_observer,
              OnAuthErrorChanged(
                  account_id, error,
                  signin_metrics::SourceForRefreshTokenOperation::kUnknown))
      .Times(1);
  delegate.UpdateAuthError(account_id, error);
  EXPECT_EQ(delegate.GetAuthError(account_id), error);

  // Update the error but without changing it should not fire a notification.
  delegate.UpdateAuthError(account_id, error);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Change the error.
  EXPECT_CALL(mock_observer,
              OnAuthErrorChanged(
                  account_id, GoogleServiceAuthError::AuthErrorNone(),
                  signin_metrics::SourceForRefreshTokenOperation::kUnknown))
      .Times(1);
  delegate.UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(delegate.GetAuthError(account_id),
            GoogleServiceAuthError::AuthErrorNone());

  // Update to none again should not fire any notification.
  delegate.UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

TEST_F(ProfileOAuth2TokenServiceDelegateTest,
       OnAuthErrorChangedAfterUpdatingCredentials) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  {
    testing::InSequence sequence;
    // `OnAuthErrorChanged()` is not called after adding a new account in tests.
    EXPECT_CALL(mock_observer, OnAuthErrorChanged).Times(0);
    EXPECT_CALL(mock_observer, OnRefreshTokenAvailable(account_id));
    EXPECT_CALL(mock_observer, OnEndBatchChanges());
    delegate.UpdateCredentials(account_id, "first refreshToken");
    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }

  {
    testing::InSequence sequence;
    // `OnAuthErrorChanged()` is also not called after a token is updated
    // without changing its error state.
    EXPECT_CALL(mock_observer, OnAuthErrorChanged).Times(0);
    EXPECT_CALL(mock_observer, OnRefreshTokenAvailable(account_id));
    EXPECT_CALL(mock_observer, OnEndBatchChanges());
    delegate.UpdateCredentials(account_id, "second refreshToken");
    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}
