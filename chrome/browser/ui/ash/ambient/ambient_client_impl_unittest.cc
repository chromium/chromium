// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/ambient_client_impl.h"

#include <memory>

#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/scoped_user_manager.h"

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class AmbientClientImplTest : public ChromeAshTestBase {
 public:
  AmbientClientImplTest() = default;
  ~AmbientClientImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kAmbientModeFeature);
    // Needed by ash.
    ambient_client_ = std::make_unique<AmbientClientImpl>();

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, base::UTF8ToUTF16(kTestProfileName),
        /*avatar_id=*/0, /*supervised_user_id=*/{},
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());

    AshTestBase::SetUp();
  }

  void TearDown() override {
    ambient_client_.reset();
    user_manager_enabler_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TestingProfile* profile() { return profile_; }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void AddAndLoginUser(const AccountId& account_id) {
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    GetFakeUserManager()->SwitchActiveUser(account_id);
    MaybeMakeAccountAsPrimaryAccount(account_id);
  }

 private:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void MaybeMakeAccountAsPrimaryAccount(const AccountId& account_id) {
    if (!identity_test_env()->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kNotRequired)) {
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          account_id.GetUserEmail());
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<AmbientClientImpl> ambient_client_;
};

TEST_F(AmbientClientImplTest, AllowedByPrimaryUser) {
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByNonPrimaryUser) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId("user2@gmail.com", kTestGaiaId));
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByEmailDomain) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId("user@gmailtest.com", kTestGaiaId));
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}
