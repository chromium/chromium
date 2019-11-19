// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"

namespace ash {

namespace {

const char kTestAccountEmail[] = "test@test.com";

}  // namespace

class MultiUserUtilTest : public ChromeAshTestBase {
 public:
  MultiUserUtilTest() {}
  ~MultiUserUtilTest() override {}

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    fake_user_manager_ = new chromeos::FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    profile_.reset(IdentityTestEnvironmentProfileAdaptor::
                       CreateProfileForIdentityTestEnvironment()
                           .release());

    identity_test_env_adaptor_.reset(
        new IdentityTestEnvironmentProfileAdaptor(profile_.get()));
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    profile_.reset();
    ChromeAshTestBase::TearDown();
  }

  // Add a user to the identity manager with given gaia_id and email.
  std::string AddUserAndSignIn(const std::string& email) {
    AccountInfo account_info =
        identity_test_env()->MakePrimaryAccountAvailable(email);
    fake_user_manager_->AddUser(
        multi_user_util::GetAccountIdFromEmail(account_info.account_id));
    fake_user_manager_->UserLoggedIn(
        multi_user_util::GetAccountIdFromEmail(account_info.account_id),
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
            account_info.account_id),
        false /* browser_restart */, false /* is_child */);

    return account_info.account_id;
  }

  void SimulateTokenRevoked(const std::string& account_id) {
    identity_test_env()->RemoveRefreshTokenForAccount(account_id);
  }

  TestingProfile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  // |fake_user_manager_| is owned by |user_manager_enabler_|.
  chromeos::FakeChromeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserUtilTest);
};

// Test that during the session it will always return a valid account id if a
// valid profile is provided, even if this profile's refresh token has been
// revoked. (On Chrome OS we don't force to end the session in this case.)
TEST_F(MultiUserUtilTest, ReturnValidAccountIdIfTokenRevoked) {
  std::string account_id = AddUserAndSignIn(kTestAccountEmail);
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();

  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));
  EXPECT_EQ(kTestAccountEmail,
            multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail());

  SimulateTokenRevoked(account_id);

  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(account_id));
  EXPECT_EQ(kTestAccountEmail,
            multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail());
}

}  // namespace ash
