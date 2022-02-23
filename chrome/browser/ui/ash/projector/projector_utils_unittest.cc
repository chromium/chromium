// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include <memory>

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class FakeUserManagerWithLocalState : public ash::FakeChromeUserManager {
 public:
  FakeUserManagerWithLocalState()
      : test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  FakeUserManagerWithLocalState(const FakeUserManagerWithLocalState&) = delete;
  FakeUserManagerWithLocalState& operator=(
      const FakeUserManagerWithLocalState&) = delete;

 private:
  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;
};

class ScopedLogIn {
 public:
  ScopedLogIn(
      FakeUserManagerWithLocalState* fake_user_manager,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR)
      : fake_user_manager_(fake_user_manager), account_id_(account_id) {
    // Prevent access to DBus. This switch is reset in case set from test SetUp
    // due massive usage of InitFromArgv.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);

    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:  // fallthrough
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        LogIn();
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        LogInAsPublicAccount();
        break;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        LogInArcKioskApp();
        break;
      case user_manager::USER_TYPE_CHILD:
        LogInChildUser();
        return;
      case user_manager::USER_TYPE_GUEST:
        LogInGuestUser();
        return;
      default:
        NOTREACHED();
    }
  }

  ScopedLogIn(const ScopedLogIn&) = delete;
  ScopedLogIn& operator=(const ScopedLogIn&) = delete;

  ~ScopedLogIn() { fake_user_manager_->RemoveUserFromList(account_id_); }

 private:
  void LogIn() {
    fake_user_manager_->AddUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInAsPublicAccount() {
    fake_user_manager_->AddPublicAccountUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInArcKioskApp() {
    fake_user_manager_->AddArcKioskAppUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInChildUser() {
    fake_user_manager_->AddChildUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInGuestUser() {
    fake_user_manager_->AddGuestUser();
    fake_user_manager_->LoginUser(account_id_);
  }

  FakeUserManagerWithLocalState* fake_user_manager_;
  const AccountId account_id_;
};

}  // namespace

class ProjectorUtilsTest : public testing::Test {
 public:
  ProjectorUtilsTest() = default;
  ProjectorUtilsTest(const ProjectorUtilsTest&) = delete;
  ProjectorUtilsTest& operator=(const ProjectorUtilsTest&) = delete;
  ~ProjectorUtilsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>());
    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    user_manager_enabler_.reset();
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
  }

  TestingProfile* profile() { return profile_; }

  FakeUserManagerWithLocalState* GetFakeUserManager() const {
    return static_cast<FakeUserManagerWithLocalState*>(
        user_manager::UserManager::Get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // Owned by |profile_manager_|
  TestingProfile* profile_ = nullptr;
};

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_RegularAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_ActiveDirectory) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::AdFromUserEmailObjGuid(
                        profile()->GetProfileUserName(), "<obj_guid>"),
                    user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_ChildUser) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId),
                    user_manager::USER_TYPE_CHILD);

  EXPECT_TRUE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_GuestAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    GetFakeUserManager()->GetGuestAccountId(),
                    user_manager::USER_TYPE_GUEST);
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_DemoAccount) {
  ScopedLogIn login(GetFakeUserManager(), user_manager::DemoAccountId(),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_KioskAppAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile()));
}

}  // namespace ash
