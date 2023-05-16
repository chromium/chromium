// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_auth_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/sync_credentials.h"
#include "net/base/net_errors.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncAuthManagerTest : public testing::Test {
 protected:
  using AccountStateChangedCallback =
      SyncAuthManager::AccountStateChangedCallback;
  using CredentialsChangedCallback =
      SyncAuthManager::CredentialsChangedCallback;

  SyncAuthManagerTest() : identity_env_(&test_url_loader_factory_) {}

  ~SyncAuthManagerTest() override = default;

  std::unique_ptr<SyncAuthManager> CreateAuthManager() {
    return CreateAuthManager(base::DoNothing(), base::DoNothing());
  }

  std::unique_ptr<SyncAuthManager> CreateAuthManager(
      const AccountStateChangedCallback& account_state_changed,
      const CredentialsChangedCallback& credentials_changed) {
    return std::make_unique<SyncAuthManager>(identity_env_.identity_manager(),
                                             account_state_changed,
                                             credentials_changed);
  }

  std::unique_ptr<SyncAuthManager> CreateAuthManagerForLocalSync() {
    return std::make_unique<SyncAuthManager>(nullptr, base::DoNothing(),
                                             base::DoNothing());
  }

  signin::IdentityTestEnvironment* identity_env() { return &identity_env_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_env_;
};

TEST_F(SyncAuthManagerTest, ProvidesNothingInLocalSyncMode) {
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManagerForLocalSync();
  EXPECT_TRUE(auth_manager->GetActiveAccountInfo().account_info.IsEmpty());
  syncer::SyncCredentials credentials = auth_manager->GetCredentials();
  EXPECT_TRUE(credentials.email.empty());
  EXPECT_TRUE(credentials.access_token.empty());
  EXPECT_TRUE(auth_manager->access_token().empty());
  // Note: Calling RegisterForAuthNotifications or any of the Connection*()
  // methods is illegal in local Sync mode, so we don't test that.
}

TEST_F(SyncAuthManagerTest, IgnoresEventsIfNotRegistered) {
  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  // Fire some auth events. We haven't called RegisterForAuthNotifications, so
  // none of this should result in any callback calls.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  // Without RegisterForAuthNotifications, the active account should always be
  // reported as empty.
  EXPECT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());
  identity_env()->SetRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());

// ChromeOS doesn't support sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  identity_env()->ClearPrimaryAccount();
  EXPECT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

// ChromeOS doesn't support sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncAuthManagerTest, ForwardsPrimaryAccountEvents) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  // Sign out of the account.
  EXPECT_CALL(account_state_changed, Run());
  // Note: The ordering of removing the refresh token and the actual sign-out is
  // undefined, see comment on IdentityManager::Observer. So we might or might
  // not get a |credentials_changed| call here.
  EXPECT_CALL(credentials_changed, Run()).Times(testing::AtMost(1));
  identity_env()->ClearPrimaryAccount();
  EXPECT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());

  // Sign in to a different account.
  EXPECT_CALL(account_state_changed, Run());
  CoreAccountId second_account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  EXPECT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            second_account_id);
}

TEST_F(SyncAuthManagerTest, NotifiesOfSignoutBeforeAccessTokenIsGone) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), base::DoNothing());

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  // Make sure an access token is available.
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Sign out of the account.
  EXPECT_CALL(account_state_changed, Run()).WillOnce([&]() {
    // At the time the callback gets run, the access token should still be here.
    EXPECT_FALSE(auth_manager->GetCredentials().access_token.empty());
  });
  identity_env()->ClearPrimaryAccount();
  // After the signout is complete, the access token should be gone.
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
  ASSERT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Unconsented primary accounts are only supported on Win/Mac/Linux.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(SyncAuthManagerTest, ForwardsUnconsentedAccountEvents) {
  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());
  auth_manager->RegisterForAuthNotifications();

  ASSERT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());

  // Make a primary account available without Sync consent.
  EXPECT_CALL(account_state_changed, Run());
  AccountInfo account_info = identity_env()->MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);

  EXPECT_FALSE(auth_manager->GetActiveAccountInfo().is_sync_consented);
  EXPECT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_info.account_id);

  // Make the account Sync-consented.
  EXPECT_CALL(account_state_changed, Run());
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_env()->identity_manager()->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(account_info.account_id,
                                             signin::ConsentLevel::kSync);

  EXPECT_TRUE(auth_manager->GetActiveAccountInfo().is_sync_consented);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_IOS)

// ChromeOS doesn't support sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncAuthManagerTest, ClearsAuthErrorOnSignoutWithRefreshTokenRemoval) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);
  ASSERT_EQ(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);

  // Sign out of the account.
  // The ordering of removing the refresh token and the actual sign-out is
  // undefined, see comment on IdentityManager::Observer. Here, explicitly
  // revoke the refresh token first (see also other test below which does *not*
  // remove the refresh token first).
  identity_env()->RemoveRefreshTokenForPrimaryAccount();

  // Note: Things are now in an intermediate state, where the primary account
  // still exists, but doesn't have a refresh token anymore. It doesn't really
  // matter whether the auth error is still there at this point.

  // Actually signing out, i.e. removing the primary account, should clear the
  // auth error, since it's now not meaningful anymore.
  identity_env()->ClearPrimaryAccount();
  EXPECT_EQ(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);
}

TEST_F(SyncAuthManagerTest,
       ClearsAuthErrorOnSignoutWithoutRefreshTokenRemoval) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);
  ASSERT_EQ(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);

  // Sign out of the account.
  // The ordering of removing the refresh token and the actual sign-out is
  // undefined, see comment on IdentityManager::Observer. Here, do *not* remove
  // the refresh token first (see also other test above which does remove it).

  // Signing out, i.e. removing the primary account, should clear the auth
  // error, since it's now not meaningful anymore.
  identity_env()->ClearPrimaryAccount();
  EXPECT_EQ(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncAuthManagerTest, DoesNotClearAuthErrorOnSyncDisable) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);
  ASSERT_EQ(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);

  auth_manager->ConnectionOpened();

  // Force an auth error by revoking the refresh token.
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_NE(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);

  // Now Sync gets turned off, e.g. because the user disabled it.
  auth_manager->ConnectionClosed();

  // Since the user is still signed in, the auth error should have remained.
  EXPECT_NE(auth_manager->GetLastAuthError().state(),
            GoogleServiceAuthError::NONE);
}

TEST_F(SyncAuthManagerTest, ForwardsCredentialsEvents) {
  // Start out already signed in before the SyncAuthManager is created.
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  auth_manager->RegisterForAuthNotifications();

  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  // Once an access token is available, the callback should get run.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now the refresh token gets updated. The access token will get dropped, so
  // this should cause another notification.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->SetRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(auth_manager->GetCredentials().access_token.empty());

  // Once a new token is available, there's another notification.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token_2");

  // Revoking the refresh token should also cause the access token to get
  // dropped.
  // Note: On ChromeOS-Ash, setting an invalid refresh token causes 2
  // "credentials changed" events, one for the token change itself, and another
  // one for the auth error caused by the invalid token.
  EXPECT_CALL(credentials_changed, Run()).Times(testing::AtLeast(1));
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
}

TEST_F(SyncAuthManagerTest, RequestsAccessTokenOnSyncStartup) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(auth_manager->GetCredentials().access_token, "access_token");
}

TEST_F(SyncAuthManagerTest,
       RetriesAccessTokenFetchWithBackoffOnTransientFailure) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  // The access token fetch should get retried (with backoff, hence no actual
  // request yet), without exposing an auth error.
  EXPECT_TRUE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
}

TEST_F(SyncAuthManagerTest,
       RetriesAccessTokenFetchWithoutBackoffOnceOnFirstCancelTransientFailure) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Expect no backoff the first time the request is canceled.
  EXPECT_FALSE(auth_manager->IsRetryingAccessTokenFetchForTest());

  // Cancel the retry as well.
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Expect retry with backoff when the first retry was also canceled.
  EXPECT_TRUE(auth_manager->IsRetryingAccessTokenFetchForTest());
}

TEST_F(SyncAuthManagerTest,
       RetriesAccessTokenFetchOnFirstCancelTransientFailure) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Expect no backoff the first time the request is canceled.
  EXPECT_FALSE(auth_manager->IsRetryingAccessTokenFetchForTest());

  // Retry is a success.
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  // Don't expect any backoff when the retry is a success.
  EXPECT_FALSE(auth_manager->IsRetryingAccessTokenFetchForTest());
}

TEST_F(SyncAuthManagerTest, AbortsAccessTokenFetchOnPersistentFailure) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();

  GoogleServiceAuthError auth_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      auth_error);

  // Auth error should get exposed; no retry.
  EXPECT_FALSE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(), auth_error);
}

TEST_F(SyncAuthManagerTest, FetchesNewAccessTokenWithBackoffOnServerError) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // The server is returning AUTH_ERROR - maybe something's wrong with the
  // token we got.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  // The access token fetch should get retried (with backoff, hence no actual
  // request yet), without exposing an auth error.
  EXPECT_TRUE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
}

TEST_F(SyncAuthManagerTest, DoesNotExposeServerError) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now a server error happens.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_SERVER_ERROR);

  // The error should not be reported as it is transient.
  EXPECT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(auth_manager->GetCredentials().access_token, "access_token");
}

TEST_F(SyncAuthManagerTest, ClearsServerErrorOnSyncDisable) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // The server returns an auth error.
  GoogleServiceAuthError auth_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      auth_error);

  ASSERT_NE(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // Now Sync gets turned off, e.g. because the user disabled it.
  auth_manager->ConnectionClosed();

  // This should not have cleared the auth error.
  EXPECT_NE(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
}

TEST_F(SyncAuthManagerTest, RequestsNewAccessTokenOnExpiry) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the token expires, resulting in an auth error from the server.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  // Should immediately drop the access token and fetch a new one (no backoff).
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(auth_manager->GetCredentials().access_token, "access_token_2");
}

TEST_F(SyncAuthManagerTest, RequestsNewAccessTokenOnRefreshTokenUpdate) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the refresh token changes.
  identity_env()->SetRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and fetch a new one (no backoff).
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(auth_manager->GetCredentials().access_token, "access_token_2");
}

TEST_F(SyncAuthManagerTest, DoesNotRequestAccessTokenAutonomously) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  // Do *not* call ConnectionStatusChanged here (which is what usually kicks off
  // the token fetch).

  // Now the refresh token gets updated. If we already had an access token
  // before, then this should trigger a new fetch. But since that initial fetch
  // never happened (e.g. because Sync is turned off), this should do nothing.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->SetRefreshTokenForPrimaryAccount();

  // Make sure no access token request was sent. Since the request goes through
  // posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
}

TEST_F(SyncAuthManagerTest, ClearsCredentialsOnRefreshTokenRemoval) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the refresh token gets revoked. No new access token should get
  // requested due to this.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and expose an auth error.
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
  EXPECT_NE(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // No new access token should have been requested. Since the request goes
  // through posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncAuthManagerTest, ClearsCredentialsOnInvalidRefreshToken) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But now an invalid refresh token gets set, i.e. we enter the "Sync paused"
  // state. No new access token should get requested due to this.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and expose a special auth error.
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
  GoogleServiceAuthError invalid_token_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  EXPECT_EQ(auth_manager->GetLastAuthError(), invalid_token_error);
  EXPECT_TRUE(auth_manager->IsSyncPaused());

  // No new access token should have been requested. Since the request goes
  // through posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncAuthManagerTest, EntersPausedStateOnPersistentAuthError) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But now an auth error happens.
  identity_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      auth_manager->GetActiveAccountInfo().account_info.account_id,
      GoogleServiceAuthError::FromServiceError("Test error"));

  // Should immediately drop the access token and enter the sync-paused state.
  EXPECT_TRUE(auth_manager->GetCredentials().access_token.empty());
  EXPECT_TRUE(auth_manager->GetLastAuthError().IsPersistentError());
  EXPECT_TRUE(auth_manager->IsSyncPaused());
}

TEST_F(SyncAuthManagerTest,
       RequestsAccessTokenWhenInvalidRefreshTokenResolved) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  // Sync starts up normally.
  auth_manager->ConnectionOpened();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But now an invalid refresh token gets set, i.e. we enter the "Sync paused"
  // state.
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(auth_manager->GetCredentials().access_token.empty());
  ASSERT_TRUE(auth_manager->IsSyncPaused());

  // Once the user signs in again and we have a valid refresh token, we should
  // also request a new access token.
  identity_env()->SetRefreshTokenForPrimaryAccount();
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::Hours(1));
  ASSERT_EQ(auth_manager->GetCredentials().access_token, "access_token_2");
}

TEST_F(SyncAuthManagerTest, DoesNotRequestAccessTokenIfSyncInactive) {
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  // Sync is *not* enabled; in particular we don't call ConnectionOpened().

  // An invalid refresh token gets set, i.e. we enter the "Sync paused" state
  // (only from SyncAuthManager's point of view - Sync as a whole is still
  // disabled).
  // Note: Depending on the exact sequence of IdentityManager::Observer calls
  // (refresh token changed and/or auth error changed), the credentials-changed
  // callback might get run multiple times.
  EXPECT_CALL(credentials_changed, Run()).Times(testing::AtLeast(1));
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(auth_manager->GetCredentials().access_token.empty());
  ASSERT_TRUE(auth_manager->IsSyncPaused());

  // Once the user signs in again and we have a valid refresh token, we should
  // *not* request a new access token, since Sync isn't active.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  // This *should* notify about changed credentials though, so that the
  // SyncService can decide to start syncing.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->SetRefreshTokenForPrimaryAccount();
  ASSERT_FALSE(auth_manager->IsSyncPaused());

  // Since the access token request goes through posted tasks, we have to spin
  // the message loop to make sure it didn't happen.
  base::RunLoop().RunUntilIdle();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Primary account with no sync consent is not supported on Android and iOS.
TEST_F(SyncAuthManagerTest, PrimaryAccountWithNoSyncConsent) {
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();

  ASSERT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());

  // Make a primary account with no sync consent available.
  AccountInfo account_info = identity_env()->MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);

  // Since unconsented primary account support is enabled, SyncAuthManager
  // should have picked up this account.
  EXPECT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_info.account_id);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Primary account with no sync consent is not supported on Android and iOS.
// On CrOS the unconsented primary account can't be changed or removed, but can
// be granted sync consent.
TEST_F(SyncAuthManagerTest, PicksNewPrimaryAccountWithSyncConsent) {
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();

  ASSERT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());

  // Make a primary account with no sync consent available.
  AccountInfo unconsented_primary_account_info =
      identity_env()->MakePrimaryAccountAvailable(
          "test@email.com", signin::ConsentLevel::kSignin);
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            unconsented_primary_account_info.account_id);

  // Once a primary account with sync consent becomes available, the unconsented
  // primary account should be overridden.
  AccountInfo primary_account_info =
      identity_env()->MakePrimaryAccountAvailable("primary@email.com",
                                                  signin::ConsentLevel::kSync);
  EXPECT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            primary_account_info.account_id);
}

TEST_F(SyncAuthManagerTest,
       DropsAccountWhenPrimaryAccountWithNoSyncConsentGoesAway) {
  std::unique_ptr<SyncAuthManager> auth_manager = CreateAuthManager();
  auth_manager->RegisterForAuthNotifications();

  // Make a primary account with no sync consent available.
  AccountInfo account_info = identity_env()->MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_info.account_id);

  identity_env()->ClearPrimaryAccount();
  EXPECT_TRUE(
      auth_manager->GetActiveAccountInfo().account_info.account_id.empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_IOS)

TEST_F(SyncAuthManagerTest, DetectsInvalidRefreshTokenAtStartup) {
  // There is a primary account, but it has an invalid refresh token (with a
  // persistent auth error).
  CoreAccountId account_id =
      identity_env()
          ->MakePrimaryAccountAvailable("test@email.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // On initialization, SyncAuthManager should pick up the auth error. This
  // should not result in a notification.
  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);

  std::unique_ptr<SyncAuthManager> auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());
  auth_manager->RegisterForAuthNotifications();
  ASSERT_EQ(auth_manager->GetActiveAccountInfo().account_info.account_id,
            account_id);

  EXPECT_TRUE(auth_manager->GetLastAuthError().IsPersistentError());
}

}  // namespace

}  // namespace syncer
