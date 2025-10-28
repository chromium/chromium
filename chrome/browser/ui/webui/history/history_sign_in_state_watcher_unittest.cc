// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"

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
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, callback.Get());

  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
}

TEST_P(HistorySignInStateWatcherTest, NullServices) {
  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistorySignInStateWatcher watcher(nullptr, nullptr, callback.Get());

  if (GetParam()) {
    EXPECT_EQ(HistorySignInState::kSyncDisabled, watcher.GetSignInState());
  } else {
    EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
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
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  ASSERT_EQ(HistorySignInState::kSyncDisabled, watcher.GetSignInState());
}

// Tabs sync is disabled by policy and there is no account info, should result
// in a sync disabled state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, TabsSyncDisabled) {
  sync_service_.GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kTabs, true);
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  ASSERT_EQ(HistorySignInState::kSyncDisabled, watcher.GetSignInState());
}

// Signing in to web only, should change the state and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest, NotifiesOnWebOnlySignIn) {
  base::test::TestFuture<void> callback;
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_,
                                    callback.GetRepeatingCallback());
  ASSERT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
  identity_test_env_.MakeAccountAvailable("test@email.com",
                                          {.set_cookie = true});
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistorySignInState::kWebOnlySignedIn, watcher.GetSignInState());
}

// Signing in to Chrome, but without history and tabs, should change the state
// and trigger a notification.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       NotifiesOnChromeSignInWithoutHistoryAndTabsSync) {
  base::test::TestFuture<void> callback;
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_,
                                    callback.GetRepeatingCallback());
  ASSERT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
  const CoreAccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistorySignInState::kSignedInNotSyncingTabs,
            watcher.GetSignInState());
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

  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  EXPECT_EQ(HistorySignInState::kSyncDisabled, watcher.GetSignInState());
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

  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  ASSERT_EQ(HistorySignInState::kSignedInNotSyncingTabs,
            watcher.GetSignInState());

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_EQ(HistorySignInState::kSignedInNotSyncingTabs,
            watcher.GetSignInState());
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
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, callback.Get());
  ASSERT_EQ(HistorySignInState::kSignedInNotSyncingTabs,
            watcher.GetSignInState());

  EXPECT_CALL(callback, Run());
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedInSyncingTabs, watcher.GetSignInState());
}

// Users with pending sign-in have a separate state.
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

  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  EXPECT_EQ(HistorySignInState::kSignInPendingNotSyncingTabs,
            watcher.GetSignInState());

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  EXPECT_EQ(HistorySignInState::kSignInPendingSyncingTabs,
            watcher.GetSignInState());
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
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_,
                                    callback.GetRepeatingCallback());
  ASSERT_EQ(HistorySignInState::kSignedInSyncingTabs, watcher.GetSignInState());
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetSignedOut();
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
}

// History sync is disabled by the user using toggles on the settings page,
// should result in a sync disabled state.
TEST_F(HistorySignInStateWatcherSyncToSigninTest,
       HasExplicitlyDisabledHistorySync) {
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, base::DoNothing());
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  ASSERT_EQ(HistorySignInState::kSignedInSyncingTabs, watcher.GetSignInState());

  sync_service_.GetUserSettings()->SetDisabledType(
      syncer::UserSelectableType::kHistory);
  EXPECT_EQ(HistorySignInState::kSyncDisabled, watcher.GetSignInState());
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
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_,
                                    callback.GetRepeatingCallback());
  ASSERT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
  account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  ASSERT_TRUE(callback.Wait());
  EXPECT_EQ(HistorySignInState::kSignedInSyncingTabs, watcher.GetSignInState());
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySignInStateWatcherTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSyncToSignin"
                                             : "WithoutSyncToSignin";
                         });

}  // namespace
