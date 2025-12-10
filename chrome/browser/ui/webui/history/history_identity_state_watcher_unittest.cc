// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_identity_state_watcher.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::StrictMock;

class HistorySignInStateWatcherTestBase : public testing::Test {
 public:
  explicit HistorySignInStateWatcherTestBase(bool replace_sync_with_signin) {
    feature_list_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSignInPromos, replace_sync_with_signin);
    // Start with no signed-in user (this makes the TestSyncService consistent
    // with the IdentityTestEnvironment).
    sync_service_.SetSignedOut();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
};

class HistorySignInStateWatcherTest : public HistorySignInStateWatcherTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  HistorySignInStateWatcherTest()
      : HistorySignInStateWatcherTestBase(
            /*replace_sync_with_signin=*/GetParam()) {}
};

TEST_P(HistorySignInStateWatcherTest, SignedOut) {
  // Initially, user is signed out and sync is off.
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.IsSyncFeatureEnabled());

  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, callback.Get());

  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);
}

TEST_P(HistorySignInStateWatcherTest, NullServices) {
  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistoryIdentityStateWatcher watcher(nullptr, nullptr, callback.Get());

  if (GetParam()) {
    EXPECT_EQ(HistoryIdentityState::TabsSync::kDisabled,
              watcher.GetHistoryIdentityState().tab_sync);
  } else {
    EXPECT_EQ(HistoryIdentityState::SignIn::kSignedOut,
              watcher.GetHistoryIdentityState().sign_in);
  }
}

class HistorySignInStateWatcherSyncToSigninTest
    : public HistorySignInStateWatcherTestBase {
 public:
  HistorySignInStateWatcherSyncToSigninTest()
      : HistorySignInStateWatcherTestBase(/*replace_sync_with_signin=*/true) {}
};

// Sync is disabled by policy, should be reflected in the sign-in state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, SyncDisabledByPolicy) {
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.SetAllowedByEnterprisePolicy(false);
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kDisabled,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Tabs sync is disabled by policy and there is no account info, should result
// in a sync disabled state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, TabsSyncDisabled) {
  sync_service_.GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kTabs, true);
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kDisabled,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Signing in to web only, should change the state and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, NotifiesOnWebOnlySignIn) {
  base::test::TestFuture<void> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_,
                                      callback.GetRepeatingCallback());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  identity_test_env_.MakeAccountAvailable("test@email.com",
                                          {.set_cookie = true});
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistoryIdentityState::SignIn::kWebOnlySignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Signing in to Chrome, but without history and tabs, should change the state
// and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       NotifiesOnChromeSignInWithoutHistoryAndTabsSync) {
  base::test::TestFuture<void> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_,
                                      callback.GetRepeatingCallback());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Signing in to Chrome, with sync enabled but tabs sync disabled, should
// result in a sync disabled state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       SyncingWithoutTabsSyncIsSyncDisabled) {
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);

  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kDisabled,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Opting in to history should not change the state and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       DoesNotNotifyOnEnablingHistorySync) {
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);

  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  ASSERT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Opting in to tabs should change the state and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, NotifiesOnEnablingTabsSync) {
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);

  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, callback.Get());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  ASSERT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);

  EXPECT_CALL(callback, Run());
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOn,
            watcher.GetHistoryIdentityState().tab_sync);
}

// Users with pending sign-in have a separate HistorySignInState. Opting in/out
// of tabs sync changes only the TabsSyncState.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       SignInPendingCanOptInToTabsSync) {
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);
  identity_test_env_.SetInvalidRefreshTokenForPrimaryAccount();

  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignInPending,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignInPending,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOn,
            watcher.GetHistoryIdentityState().tab_sync);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Signing out again should once again change the state and trigger a
// notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, NotifiesOnSignOut) {
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  base::test::TestFuture<void> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_,
                                      callback.GetRepeatingCallback());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  ASSERT_EQ(HistoryIdentityState::TabsSync::kTurnedOn,
            watcher.GetHistoryIdentityState().tab_sync);

  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetSignedOut();
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOff,
            watcher.GetHistoryIdentityState().tab_sync);
}

// History sync is disabled by the user using toggles on the settings page,
// should result in a sync disabled state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       HasExplicitlyDisabledHistorySync) {
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_, base::DoNothing());
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  ASSERT_EQ(HistoryIdentityState::TabsSync::kTurnedOn,
            watcher.GetHistoryIdentityState().tab_sync);

  sync_service_.GetUserSettings()->SetDisabledType(
      syncer::UserSelectableType::kHistory);
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kDisabled,
            watcher.GetHistoryIdentityState().tab_sync);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class HistorySignInStateWatcherWithoutSyncToSigninTest
    : public HistorySignInStateWatcherTestBase {
 public:
  HistorySignInStateWatcherWithoutSyncToSigninTest()
      : HistorySignInStateWatcherTestBase(/*replace_sync_with_signin=*/false) {}
};

// Enabling Sync should change the state and trigger a notification.
TEST_F(HistorySignInStateWatcherWithoutSyncToSigninTest,
       NotifiesOnEnablingSync) {
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  base::test::TestFuture<void> callback;
  HistoryIdentityStateWatcher watcher(identity_test_env_.identity_manager(),
                                      &sync_service_,
                                      callback.GetRepeatingCallback());
  ASSERT_EQ(HistoryIdentityState::SignIn::kSignedOut,
            watcher.GetHistoryIdentityState().sign_in);
  account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistoryIdentityState::SignIn::kSignedIn,
            watcher.GetHistoryIdentityState().sign_in);
  EXPECT_EQ(HistoryIdentityState::TabsSync::kTurnedOn,
            watcher.GetHistoryIdentityState().tab_sync);
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySignInStateWatcherTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSyncToSignin"
                                             : "WithoutSyncToSignin";
                         });

}  // namespace
