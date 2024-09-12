// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/user_adding_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_adding_screen_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class UserAddingScreenTest : public LoginManagerTest,
                             public UserAddingScreen::Observer {
 public:
  UserAddingScreenTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(3);
  }

  UserAddingScreenTest(const UserAddingScreenTest&) = delete;
  UserAddingScreenTest& operator=(const UserAddingScreenTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    UserAddingScreen::Get()->AddObserver(this);
  }

  void WaitUntilUserAddingFinishedOrCancelled() {
    if (finished_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void OnUserAddingFinished() override {
    ++user_adding_finished_;
    finished_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnBeforeUserAddingScreenStarted() override {
    ++user_adding_started_;
    finished_ = false;
  }

  int user_adding_started() { return user_adding_started_; }

  int user_adding_finished() { return user_adding_finished_; }

  std::vector<AccountId> users_in_session_order_;
  LoginManagerMixin login_mixin_{&mixin_host_};

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  int user_adding_started_ = 0;
  int user_adding_finished_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool finished_ = false;  // True if OnUserAddingFinished() has been called
                           // before WaitUntilUserAddingFinishedOrCancelled().
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, CancelAdding) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  test::ShowUserAddingScreen();

  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::ClickCancelButton());
  WaitUntilUserAddingFinishedOrCancelled();

  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
            users[0].account_id);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, UILogin) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  test::ShowUserAddingScreen();

  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  SetExpectedCredentials(CreateUserContext(users.back().account_id, kPassword));
  LoginScreenTestApi::SubmitPassword(users.back().account_id, kPassword,
                                     true /* check_if_submittable */);

  WaitUntilUserAddingFinishedOrCancelled();
  EXPECT_EQ(user_adding_finished(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
  ASSERT_EQ(user_manager->GetLoggedInUsers().size(), 2u);

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, AddingSeveralUsers) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[0].account_id);
  users_in_session_order_.push_back(users[0].account_id);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const int n = users.size();
  for (int i = 1; i < n; ++i) {
    test::ShowUserAddingScreen();

    EXPECT_EQ(user_adding_started(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_SECONDARY);

    AddUser(users[i].account_id);
    users_in_session_order_.push_back(users[i].account_id);
    WaitUntilUserAddingFinishedOrCancelled();

    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(user_manager->GetLoggedInUsers().size(),
              static_cast<size_t>(i + 1));
  }

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  // Now check how unlock policy works for these users.
  PrefService* prefs1 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[0])
          ->GetPrefs();
  PrefService* prefs2 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[1])
          ->GetPrefs();
  PrefService* prefs3 =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager->GetLoggedInUsers()[2])
          ->GetPrefs();
  ASSERT_TRUE(prefs1 != nullptr);
  ASSERT_TRUE(prefs2 != nullptr);
  ASSERT_TRUE(prefs3 != nullptr);
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, false);
  prefs3->SetBoolean(prefs::kEnableAutoScreenLock, false);

  // One of the users has the primary-only policy.
  // List of unlock users doesn't depend on kEnableLockScreen preference.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, true);
  prefs1->SetString(user_manager::prefs::kMultiProfileUserBehaviorPref,
                    user_manager::MultiUserSignInPolicyToPrefValue(
                        user_manager::MultiUserSignInPolicy::kPrimaryOnly));
  prefs2->SetString(user_manager::prefs::kMultiProfileUserBehaviorPref,
                    user_manager::MultiUserSignInPolicyToPrefValue(
                        user_manager::MultiUserSignInPolicy::kUnrestricted));
  prefs3->SetString(user_manager::prefs::kMultiProfileUserBehaviorPref,
                    user_manager::MultiUserSignInPolicyToPrefValue(
                        user_manager::MultiUserSignInPolicy::kUnrestricted));
  user_manager::UserList unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 1u);
  EXPECT_EQ(users[0].account_id, unlock_users[0]->GetAccountId());

  // If all users have unrestricted policy then anyone can perform unlock.
  prefs1->SetString(user_manager::prefs::kMultiProfileUserBehaviorPref,
                    user_manager::MultiUserSignInPolicyToPrefValue(
                        user_manager::MultiUserSignInPolicy::kUnrestricted));
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  }

  // This preference doesn't affect list of unlock users.
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, true);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 3u);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  }

  // Now one of the users is unable to unlock.
  prefs3->SetBoolean(ash::prefs::kAllowScreenLock, false);
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  }
  prefs3->SetBoolean(ash::prefs::kAllowScreenLock, true);

  // Now one of the users has not-allowed policy.
  // In this scenario this user is not allowed in multi-profile session but
  // if that user happened to still be part of multi-profile session it should
  // not be listed on screen lock.
  prefs3->SetString(user_manager::prefs::kMultiProfileUserBehaviorPref,
                    user_manager::MultiUserSignInPolicyToPrefValue(
                        user_manager::MultiUserSignInPolicy::kNotAllowed));
  unlock_users = user_manager->GetUnlockUsers();
  ASSERT_EQ(unlock_users.size(), 2u);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(users_in_session_order_[i], unlock_users[i]->GetAccountId());
  }
}

// TODO(crbug.com/40059904) Disabled
// UserAddingScreenTest.ScreenVisibilityAfterLockbecause it is failing on "Linux
// Chromium OS ASan LSan Tests" builder.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ScreenVisibilityAfterLock DISABLED_ScreenVisibilityAfterLock
#else
#define MAYBE_ScreenVisibilityAfterLock ScreenVisibilityAfterLock
#endif
IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, MAYBE_ScreenVisibilityAfterLock) {
  const auto& users = login_mixin_.users();
  LoginUser(users[0].account_id);

  ScreenLockerTester screen_locker_tester;
  screen_locker_tester.Lock();
  ScreenLocker::Hide();
  screen_locker_tester.WaitForUnlock();

  UserAddingScreen::Get()->Start();
  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);

  EXPECT_TRUE(LoginScreenTestApi::IsCancelButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::ClickCancelButton());

  WaitUntilUserAddingFinishedOrCancelled();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, InfoBubbleVisible) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  EXPECT_FALSE(LoginScreenTestApi::IsUserAddingScreenIndicatorShown());

  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Check if the user adding screen bubble is still shown after adding other
  // users, as a re-layout is executed each time.
  const int n = users.size();
  for (int i = 1; i < n; ++i) {
    UserAddingScreen::Get()->Start();
    EXPECT_EQ(user_adding_started(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::LOGIN_SECONDARY);

    EXPECT_TRUE(LoginScreenTestApi::IsUserAddingScreenIndicatorShown());

    AddUser(users[i].account_id);
    users_in_session_order_.push_back(users[i].account_id);
    WaitUntilUserAddingFinishedOrCancelled();

    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
    EXPECT_TRUE(LoginDisplayHost::default_host() == nullptr);
    ASSERT_EQ(user_manager->GetLoggedInUsers().size(),
              static_cast<size_t>(i + 1));
  }

  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

// Makes sure Chrome doesn't crash if we lock the screen during an add-user
// flow. Regression test for crbug.com/467111.
// Note that this test has been moved from ScreenLockerTest because it is easier
// to login a user here; and without any logged user on the user adding screen,
// a OOBE dialog would appear, making the test crash.
IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, LockScreenWhileAddingUser) {
  const auto& users = login_mixin_.users();
  EXPECT_EQ(users.size(), user_manager::UserManager::Get()->GetUsers().size());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  LoginUser(users[0].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);

  UserAddingScreen::Get()->Start();
  EXPECT_EQ(user_adding_started(), 1);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_SECONDARY);
  base::RunLoop().RunUntilIdle();

  ScreenLocker::HandleShowLockScreenRequest();
}

}  // namespace ash
