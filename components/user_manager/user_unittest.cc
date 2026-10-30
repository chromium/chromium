// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

namespace {

const char kEmail[] = "email@example.com";
const GaiaId::Literal kGaiaId("fake_gaia_id");

}  // namespace

TEST(UserTest, DeviceLocalAccountAffiliation) {
  // This implementation of RAII for User* is to prevent memory leak.
  // Smart pointers are not friends of User and can't call protected destructor.
  class ScopedUser {
   public:
    ScopedUser(const User* const user) : user_(user) {}

    ScopedUser(const ScopedUser&) = delete;
    ScopedUser& operator=(const ScopedUser&) = delete;

    ~ScopedUser() { delete user_; }

    bool IsAffiliated() const { return user_ && user_->IsAffiliated(); }

   private:
    const raw_ptr<const User, DanglingUntriaged> user_;
  };

  const AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);

  ScopedUser kiosk_user(User::CreateKioskChromeAppUser(account_id));
  EXPECT_TRUE(kiosk_user.IsAffiliated());

  ScopedUser public_session_user(User::CreatePublicAccountUser(account_id));
  EXPECT_TRUE(public_session_user.IsAffiliated());

  ScopedUser web_kiosk_user(User::CreateKioskWebAppUser(account_id));
  EXPECT_TRUE(web_kiosk_user.IsAffiliated());

  ScopedUser arcvm_kiosk_user(User::CreateKioskArcvmAppUser(account_id));
  EXPECT_TRUE(arcvm_kiosk_user.IsAffiliated());
}

TEST(UserTest, GetDirName) {
  // Regular case.
  EXPECT_EQ("u-abcde123456", User::GetDirName("abcde123456"));

  // Special cases for stub users, which are used as is.
  EXPECT_EQ("user", User::GetDirName("user"));
  EXPECT_EQ("test-user", User::GetDirName("test-user"));
}

TEST(UserDeathTest, GetDirNameWithEmptyHash) {
  EXPECT_CHECK_DEATH(User::GetDirName(""));
}

class UserGetPathTest : public testing::Test {
 public:
  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
  }

  void TearDown() override { user_session_test_environment_.reset(); }

 protected:
  ash::test::UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride user_data_dir_override_{ash::DIR_USER_DATA};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(UserGetPathTest, GetPath) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);
  User* user = user_session_test_environment().AddRegularUser(account_id);
  ASSERT_TRUE(user);

  // username_hash is not yet set before the user's home directory is mounted.
  ASSERT_TRUE(user->username_hash().empty());
  EXPECT_EQ(std::nullopt, user->GetPath());

  user_session_test_environment().LogIn(account_id);
  ASSERT_FALSE(user->username_hash().empty());
  EXPECT_EQ(base::PathService::CheckedGet(ash::DIR_USER_DATA)
                .Append("u-" + user->username_hash()),
            user->GetPath());
}

}  // namespace user_manager
