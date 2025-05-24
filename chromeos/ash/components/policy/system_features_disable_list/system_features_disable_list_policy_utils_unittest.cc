// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/system_features_disable_list/system_features_disable_list_policy_utils.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/system_features_disable_list_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kUserEmail[] = "user@example.com";
constexpr GaiaId::Literal kUserGaiaId("user123");
constexpr char kMGSAccountId[] = "mgs123";

}  // namespace

class SystemFeaturesDisableListPolicyUtilsTest : public testing::Test {
 public:
  SystemFeaturesDisableListPolicyUtilsTest()
      : cros_settings_(std::make_unique<ash::CrosSettings>()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {chromeos::features::kSystemFeaturesDisableListHidden},
        /*disabled_features=*/{});
  }
  ~SystemFeaturesDisableListPolicyUtilsTest() override = default;

 protected:
  void SetUp() override {
    auto provider =
        std::make_unique<ash::FakeCrosSettingsProvider>(base::DoNothing());
    provider->Set(ash::kAccountsPrefShowUserNamesOnSignIn, true);
    cros_settings_->AddSettingsProvider(std::move(provider));

    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    RegisterDisabledSystemFeaturesPrefs(local_state_.registry());

    // Register users
    const auto user_account_id =
        AccountId::FromUserEmailGaiaId(kUserEmail, kUserGaiaId);
    user_manager::TestHelper::RegisterPersistedUser(local_state_,
                                                    user_account_id);

    const auto mgs_user_id = policy::GenerateDeviceLocalAccountUserId(
        kMGSAccountId, DeviceLocalAccountType::kPublicSession);
    user_manager::TestHelper::RegisterPublicAccountUser(local_state_,
                                                        mgs_user_id);

    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        &local_state_, cros_settings_.get());
    user_manager_->Initialize();

    user_manager_->SetUserPolicyStatus(user_account_id,
                                       /*is_managed=*/true,
                                       /*is_affiliated=*/true);
  }

  void TearDown() override {
    user_manager_->Destroy();
    user_manager_.reset();
    cros_settings_.reset();
  }

  const PrefService& local_state() { return local_state_; }

  // Helper function to set the system features disable mode preference.
  void SetSystemFeaturesDisableModePref(const std::string& value) {
    local_state_.SetString(policy::policy_prefs::kSystemFeaturesDisableMode,
                           value);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::CrosSettings> cros_settings_;
  std::unique_ptr<user_manager::UserManager> user_manager_;
};

TEST_F(SystemFeaturesDisableListPolicyUtilsTest, RegularUser_PrefNotSet) {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, kUserGaiaId);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

  EXPECT_TRUE(IsDisabledAppsModeHidden(local_state()));
}

TEST_F(SystemFeaturesDisableListPolicyUtilsTest, RegularUser_PrefIgnoredIfSet) {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, kUserGaiaId);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

  SetSystemFeaturesDisableModePref(policy::kSystemFeaturesDisableModeBlocked);

  EXPECT_TRUE(IsDisabledAppsModeHidden(local_state()));
}

TEST_F(SystemFeaturesDisableListPolicyUtilsTest,
       ManagedGuestSession_PolicyDefault) {
  const auto mgs_user_id = policy::GenerateDeviceLocalAccountUserId(
      kMGSAccountId, DeviceLocalAccountType::kPublicSession);
  const auto mgs_account_id = AccountId::FromUserEmail(mgs_user_id);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(
      mgs_account_id,
      user_manager::TestHelper::GetFakeUsernameHash(mgs_account_id));

  EXPECT_FALSE(IsDisabledAppsModeHidden(local_state()));
}

TEST_F(SystemFeaturesDisableListPolicyUtilsTest,
       ManagedGuestSession_PolicyHidden) {
  const auto mgs_user_id = policy::GenerateDeviceLocalAccountUserId(
      kMGSAccountId, DeviceLocalAccountType::kPublicSession);
  const auto mgs_account_id = AccountId::FromUserEmail(mgs_user_id);
  auto* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(
      mgs_account_id,
      user_manager::TestHelper::GetFakeUsernameHash(mgs_account_id));

  SetSystemFeaturesDisableModePref(policy::kSystemFeaturesDisableModeHidden);

  EXPECT_TRUE(IsDisabledAppsModeHidden(local_state()));
}

}  // namespace policy
