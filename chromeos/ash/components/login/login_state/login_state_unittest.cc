// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/login_state/login_state.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class LoginStateTest : public testing::Test, public LoginState::Observer {
 public:
  LoginStateTest()
      : logged_in_user_type_(LoginState::LOGGED_IN_USER_NONE),
        login_state_changes_count_(0) {}

  LoginStateTest(const LoginStateTest&) = delete;
  LoginStateTest& operator=(const LoginStateTest&) = delete;

  ~LoginStateTest() override = default;

  // testing::Test
  void SetUp() override {
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);
    LoginState::Get()->AddObserver(this);
  }

  void TearDown() override {
    LoginState::Get()->RemoveObserver(this);
    LoginState::Shutdown();
  }

  // LoginState::Observer
  void LoggedInStateChanged() override {
    ++login_state_changes_count_;
    logged_in_user_type_ = LoginState::Get()->GetLoggedInUserType();
  }

 protected:
  // Returns number of times the login state changed since the last call to
  // this method.
  unsigned int GetNewLoginStateChangesCount() {
    unsigned int result = login_state_changes_count_;
    login_state_changes_count_ = 0;
    return result;
  }

  LoginState::LoggedInUserType logged_in_user_type_;

 private:
  unsigned int login_state_changes_count_;
};

TEST_F(LoginStateTest, TestLoginState) {
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());

  // Setting login state to ACTIVE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());

  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR, logged_in_user_type_);
}

TEST_F(LoginStateTest, TestSafeModeLoginState) {
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());
  // Setting login state to SAFE MODE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_SAFE_MODE,
                                      LoginState::LOGGED_IN_USER_NONE);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);

  // Setting login state to ACTIVE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());

  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR, logged_in_user_type_);
}

TEST_F(LoginStateTest, TestLoggedInStateChangedObserverOnUserTypeChange) {
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);

  EXPECT_EQ(1u, GetNewLoginStateChangesCount());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR,
            LoginState::Get()->GetLoggedInUserType());

  // Change the user type, without changing the logged in state.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_CHILD);

  EXPECT_EQ(1u, GetNewLoginStateChangesCount());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_CHILD, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_CHILD,
            LoginState::Get()->GetLoggedInUserType());
}

TEST_F(LoginStateTest, TestPrimaryUser) {
  const AccountId account_id = AccountId::FromUserEmail("test@test");
  std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
  fake_user_manager->AddUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, username_hash,
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  EXPECT_EQ(username_hash, LoginState::Get()->primary_user_hash());
}

}  // namespace ash
