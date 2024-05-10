// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/webui/ash/settings/pages/reset/reset_section.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

// #include "components/user_manager/fake_chrome_user_manager.h"
namespace ash::settings {

class OsSettingsFeaturesUtilTest : public testing::Test {
 public:
  OsSettingsFeaturesUtilTest() = default;
  ~OsSettingsFeaturesUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void TearDown() override { fake_user_manager_.Reset(); }

  ash::FakeChromeUserManager* FakeChromeUserManager() {
    return fake_user_manager_.Get();
  }

  const AccountId MakeAccountId() {
    return AccountId::FromUserEmailGaiaId("test-user@testdomain.com",
                                          "1234567890");
  }

  ash::StubInstallAttributes& stub_install_attributes() {
    return *stub_install_attributes_.Get();
  }

 private:
  ash::ScopedStubInstallAttributes stub_install_attributes_;
  // Owned by |scoped_user_manager_|.
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(OsSettingsFeaturesUtilTest, PowerwashAllowedForRegularUser) {
  const AccountId account_id = MakeAccountId();
  auto* fake_chrome_user_manager_ = FakeChromeUserManager();
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->LoginUser(account_id);

  EXPECT_TRUE(IsPowerwashAllowed());
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForGuestUser) {
  auto* fake_chrome_user_manager_ = FakeChromeUserManager();
  auto* user = fake_chrome_user_manager_->AddGuestUser();
  fake_chrome_user_manager_->LoginUser(user->GetAccountId());

  EXPECT_FALSE(IsPowerwashAllowed());
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForChildUser) {
  const AccountId account_id = MakeAccountId();
  auto* fake_chrome_user_manager_ = FakeChromeUserManager();
  fake_chrome_user_manager_->AddChildUser(account_id);
  fake_chrome_user_manager_->set_current_user_child(true);
  fake_chrome_user_manager_->LoginUser(account_id);

  EXPECT_FALSE(IsPowerwashAllowed());
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForManagedUser) {
  stub_install_attributes().SetCloudManaged("fake-managed.com", "device-id");

  const AccountId account_id = MakeAccountId();
  auto* fake_chrome_user_manager_ = FakeChromeUserManager();
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->LoginUser(account_id);

  EXPECT_FALSE(IsPowerwashAllowed());
}

}  // namespace ash::settings
