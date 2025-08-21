// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_sign_in_state_watcher.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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

  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
}

class HistorySignInStateWatcherSyncToSigninTest
    : public HistorySignInStateWatcherTestBase {
 public:
  HistorySignInStateWatcherSyncToSigninTest()
      : HistorySignInStateWatcherTestBase(/*replace_sync_with_signin=*/true) {}
};

TEST_F(HistorySignInStateWatcherSyncToSigninTest, NotifiesAboutStateChanges) {
  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, callback.Get());
  ASSERT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());

  // Signing in, but without history, should not change the state, nor trigger a
  // notification yet.
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());

  // Opting in to history should change the state and trigger a notification.
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_CALL(callback, Run());
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedIn, watcher.GetSignInState());

#if !BUILDFLAG(IS_CHROMEOS)
  // Signing out again should once again change the state and trigger a
  // notification.
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetSignedOut();
  EXPECT_CALL(callback, Run());
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

class HistorySignInStateWatcherWithoutSyncToSigninTest
    : public HistorySignInStateWatcherTestBase {
 public:
  HistorySignInStateWatcherWithoutSyncToSigninTest()
      : HistorySignInStateWatcherTestBase(/*replace_sync_with_signin=*/false) {}
};

TEST_F(HistorySignInStateWatcherWithoutSyncToSigninTest,
       NotifiesAboutStateChanges) {
  StrictMock<base::MockCallback<base::RepeatingClosure>> callback;
  HistorySignInStateWatcher watcher(identity_test_env_.identity_manager(),
                                    &sync_service_, callback.Get());
  ASSERT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());

  // Just signing in (without enabling Sync) should *not* change the state, nor
  // trigger a notification.
  CoreAccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());

  // Enabling Sync should change the state and trigger a notification.
  account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  EXPECT_CALL(callback, Run());
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedIn, watcher.GetSignInState());

#if !BUILDFLAG(IS_CHROMEOS)
  // Signing out and disabling Sync again should once again change the state and
  // trigger a notification.
  sync_service_.SetSignedOut();
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_CALL(callback, Run());
  sync_service_.FireStateChanged();
  EXPECT_EQ(HistorySignInState::kSignedOut, watcher.GetSignInState());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

INSTANTIATE_TEST_SUITE_P(,
                         HistorySignInStateWatcherTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSyncToSignin"
                                             : "WithoutSyncToSignin";
                         });

}  // namespace
