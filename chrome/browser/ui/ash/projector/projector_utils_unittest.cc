// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestEmail[] = "user@gmail.com";
constexpr GaiaId::Literal kTestGaiaId("1234567890");

}  // namespace

class ProjectorUtilsTest : public testing::Test {
 public:
  ProjectorUtilsTest() = default;
  ProjectorUtilsTest(const ProjectorUtilsTest&) = delete;
  ProjectorUtilsTest& operator=(const ProjectorUtilsTest&) = delete;
  ~ProjectorUtilsTest() override = default;

  void SetUp() override {
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override { user_session_test_environment_.reset(); }

  test::UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

  // Logs in the given `user`, and returns the Profile for the user.
  TestingProfile* LogIn(const user_manager::User* user) {
    CHECK(user);
    user_session_test_environment_->LogIn(user->GetAccountId());
    if (is_managed()) {
      // TODO(crbug.com/583613638): In production, the policy status is set
      // after the user logs in, but before the Profile is created.
      // UserSessionTestEnvironment does not support such a hook yet, so set
      // it after the Profile is created as an approximation.
      user_manager::UserManager::Get()->SetUserPolicyStatus(
          user->GetAccountId(), /*is_managed=*/true, /*is_affiliated=*/false);
    }
    // The Profile is created by ChromeUserSessionTestEnvironmentDelegate via
    // TestingProfileManager.
    auto* profile = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByUser(user)));
    CHECK(profile);
    SetUpProfile(profile);
    return profile;
  }

  // Called after the Profile for the logged in user is created.
  virtual void SetUpProfile(TestingProfile* profile) {}

  virtual bool is_managed() const { return false; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

class ProjectorUtilsChildTest : public ProjectorUtilsTest {
 public:
  // ProjectorUtilsTest:
  void SetUpProfile(TestingProfile* profile) override {
    profile->SetIsSupervisedProfile(true);
    profile->GetPrefs()->SetBoolean(
        prefs::kProjectorDogfoodForFamilyLinkEnabled, true);
  }

  bool is_managed() const override { return true; }
};

class ProjectorUtilsManagedTest : public ProjectorUtilsTest {
 public:
  // ProjectorUtilsTest:
  void SetUpProfile(TestingProfile* profile) override {
    profile->GetPrefs()->SetBoolean(prefs::kProjectorAllowByPolicy, true);
  }

  bool is_managed() const override { return true; }
};

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_RegularAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddRegularUser(
      AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId)));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsManagedTest, IsProjectorAllowedForProfile_ManagedAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddRegularUser(
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId)));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsChildTest, IsProjectorAllowedForProfile_ChildUser) {
  Profile* profile = LogIn(user_session_test_environment().AddChildUser(
      AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId)));
  EXPECT_TRUE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_GuestAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddGuestUser());
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_PublicAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddPublicAccountUser(
      policy::GenerateDeviceLocalAccountUserId(
          "public-account", policy::DeviceLocalAccountType::kPublicSession)));
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAllowedForProfile_KioskAppAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddKioskWebAppUser(
      policy::GenerateDeviceLocalAccountUserId(
          "web-kiosk", policy::DeviceLocalAccountType::kWebKioskApp)));
  EXPECT_FALSE(IsProjectorAllowedForProfile(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_RegularAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddRegularUser(
      AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId)));
  EXPECT_TRUE(IsProjectorAppEnabled(profile));
}

TEST_F(ProjectorUtilsManagedTest, IsProjectorAppEnabled_ManagedAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddRegularUser(
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId)));
  EXPECT_TRUE(IsProjectorAppEnabled(profile));
}

TEST_F(ProjectorUtilsChildTest, IsProjectorAppEnabled_ChildUser) {
  Profile* profile = LogIn(user_session_test_environment().AddChildUser(
      AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId)));
  EXPECT_TRUE(IsProjectorAppEnabled(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_GuestAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddGuestUser());
  EXPECT_FALSE(IsProjectorAppEnabled(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_PublicAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddPublicAccountUser(
      policy::GenerateDeviceLocalAccountUserId(
          "public-account", policy::DeviceLocalAccountType::kPublicSession)));
  EXPECT_FALSE(IsProjectorAppEnabled(profile));
}

TEST_F(ProjectorUtilsTest, IsProjectorAppEnabled_KioskAppAccount) {
  Profile* profile = LogIn(user_session_test_environment().AddKioskWebAppUser(
      policy::GenerateDeviceLocalAccountUserId(
          "web-kiosk", policy::DeviceLocalAccountType::kWebKioskApp)));
  EXPECT_FALSE(IsProjectorAppEnabled(profile));
}

}  // namespace ash
