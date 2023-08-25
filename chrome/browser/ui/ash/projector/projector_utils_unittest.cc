// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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

  raw_ptr<FakeUserManagerWithLocalState, ExperimentalAsh> fake_user_manager_;
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

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>());

    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile::Builder builder;
    builder.SetPrefService(std::move(prefs));
    if (is_child())
      builder.SetIsSupervisedProfile();
    builder.OverridePolicyConnectorIsManagedForTesting(is_managed());
    profile_ = builder.Build();
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    user_manager_enabler_.reset();
    profile_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }
  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  FakeUserManagerWithLocalState* GetFakeUserManager() const {
    return static_cast<FakeUserManagerWithLocalState*>(
        user_manager::UserManager::Get());
  }

  virtual bool is_child() const { return false; }

  virtual bool is_managed() const { return false; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

class ProjectorUtilsChildTest : public ProjectorUtilsTest {
 public:
  // ProjectorUtilsTest:
  void SetUp() override {
    ProjectorUtilsTest::SetUp();
    GetPrefs()->SetBoolean(prefs::kProjectorDogfoodForFamilyLinkEnabled, true);
  }

  bool is_child() const override { return true; }

  bool is_managed() const override { return true; }
};

class ProjectorUtilsManagedTest : public ProjectorUtilsTest {
 public:
  // ProjectorUtilsTest:
  void SetUp() override {
    ProjectorUtilsTest::SetUp();
    GetPrefs()->SetBoolean(prefs::kProjectorAllowByPolicy, true);
  }

  bool is_managed() const override { return true; }
};

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_RegularAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsManagedTest, IsProjectorAllowedForProfile_ManagedAccount) {
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsChildTest, IsProjectorAllowedForProfile_ChildUser) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId),
                    user_manager::USER_TYPE_CHILD);

  EXPECT_TRUE(IsProjectorAllowedForProfile(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_GuestAccount) {
  ScopedLogIn login(GetFakeUserManager(), user_manager::GuestAccountId(),
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

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_RegularAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(IsProjectorAppEnabled(profile()));
}

TEST_F(ProjectorUtilsManagedTest, IsProjectorAppEnabled_ManagedAccount) {
  ScopedLogIn login(
      GetFakeUserManager(),
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId));
  EXPECT_TRUE(IsProjectorAppEnabled(profile()));
}

TEST_F(ProjectorUtilsChildTest, IsProjectorAppEnabled_ChildUser) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId),
                    user_manager::USER_TYPE_CHILD);

  EXPECT_TRUE(IsProjectorAppEnabled(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_GuestAccount) {
  ScopedLogIn login(GetFakeUserManager(), user_manager::GuestAccountId(),
                    user_manager::USER_TYPE_GUEST);
  EXPECT_FALSE(IsProjectorAppEnabled(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_DemoAccount) {
  ScopedLogIn login(GetFakeUserManager(), user_manager::DemoAccountId(),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_FALSE(IsProjectorAppEnabled(profile()));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_KioskAppAccount) {
  ScopedLogIn login(GetFakeUserManager(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);
  EXPECT_FALSE(IsProjectorAppEnabled(profile()));
}

}  // namespace ash
