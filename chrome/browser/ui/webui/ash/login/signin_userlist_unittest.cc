// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const size_t kMaxUsers = 50;  // same as in user_selection_screen.cc
const char* kOwner = "owner@gmail.com";
const char* kUsersPublic[] = {"public0@gmail.com", "public1@gmail.com"};

std::string GenerateUserEmail(int number) {
  return "a" + base::NumberToString(number) + "@gmail.com";
}

}  // namespace

class SigninPrepareUserListTest : public testing::Test {
 public:
  SigninPrepareUserListTest()
      : fake_user_manager_(new FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())) {}

  SigninPrepareUserListTest(const SigninPrepareUserListTest&) = delete;
  SigninPrepareUserListTest& operator=(const SigninPrepareUserListTest&) =
      delete;

  ~SigninPrepareUserListTest() override {}

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());

    for (size_t i = 0; i < std::size(kUsersPublic); ++i)
      fake_user_manager_->AddPublicAccountUser(
          AccountId::FromUserEmail(kUsersPublic[i]));

    for (size_t i = 0; i < kMaxUsers + 1; ++i) {
      fake_user_manager_->AddUser(
          AccountId::FromUserEmail(GenerateUserEmail(i)));
      // Insert owner second to last.
      if (i == kMaxUsers - 1)
        fake_user_manager_->AddUser(AccountId::FromUserEmail(kOwner));
    }

    fake_user_manager_->SetOwnerId(AccountId::FromUserEmail(kOwner));
  }

  void TearDown() override {
    profile_manager_.reset();
    testing::Test::TearDown();
  }

  FakeChromeUserManager* user_manager() { return fake_user_manager_; }

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::map<std::string, proximity_auth::mojom::AuthType> user_auth_type_map;
};

TEST_F(SigninPrepareUserListTest, AlwaysKeepOwnerInList) {
  EXPECT_LT(kMaxUsers, user_manager()->GetUsers().size());
  user_manager::UserList users_to_send =
      UserSelectionScreen::PrepareUserListForSending(
          user_manager()->GetUsers(), AccountId::FromUserEmail(kOwner),
          true /* is_signin_to_add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ(kOwner, users_to_send.back()->GetAccountId().GetUserEmail());

  user_manager()->RemoveUserFromList(AccountId::FromUserEmail("a16@gmail.com"));
  user_manager()->RemoveUserFromList(AccountId::FromUserEmail("a17@gmail.com"));
  users_to_send = UserSelectionScreen::PrepareUserListForSending(
      user_manager()->GetUsers(), AccountId::FromUserEmail(kOwner),
      true /* is_signin_to_add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ(GenerateUserEmail(kMaxUsers),
            users_to_send.back()->GetAccountId().GetUserEmail());
  EXPECT_EQ(kOwner,
            users_to_send[kMaxUsers - 2]->GetAccountId().GetUserEmail());
}

TEST_F(SigninPrepareUserListTest, PublicAccounts) {
  user_manager::UserList users_to_send =
      UserSelectionScreen::PrepareUserListForSending(
          user_manager()->GetUsers(), AccountId::FromUserEmail(kOwner),
          true /* is_signin_to_add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ("a0@gmail.com",
            users_to_send.front()->GetAccountId().GetUserEmail());

  users_to_send = UserSelectionScreen::PrepareUserListForSending(
      user_manager()->GetUsers(), AccountId::FromUserEmail(kOwner),
      false /* is_signin_to_add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ("public0@gmail.com",
            users_to_send.front()->GetAccountId().GetUserEmail());
}

}  // namespace ash
