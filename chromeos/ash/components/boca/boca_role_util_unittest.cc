// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_role_util.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BocaRoleUtilTest : public testing::Test {
 public:
  BocaRoleUtilTest() = default;
  ~BocaRoleUtilTest() override = default;

  void SetUp() override {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    ash::boca_util::RegisterPrefs(local_state_.registry());
    user_manager_ =
        std::make_unique<user_manager::FakeUserManager>(&local_state_);
    user_manager_->Initialize();
    affiliated_user_ = user_manager_->AddGaiaUser(
        affiliated_user_account_, user_manager::UserType::kRegular);
    user_manager_->SetUserPolicyStatus(affiliated_user_account_,
                                       /*is_managed=*/true,
                                       /*is_affiliated=*/true);
    unaffiliated_user_ = user_manager_->AddGaiaUser(
        unaffiliated_user_account_, user_manager::UserType::kRegular);
  }

  void TearDown() override {
    affiliated_user_ = nullptr;
    unaffiliated_user_ = nullptr;
    user_manager_->Destroy();
    user_manager_.reset();
  }

 protected:
  template <size_t N>
  void CheckPrefsSyncableFlags(const std::array<const char*, N>& pref_names,
                               bool expect_sync) {
    for (const char* pref_name : pref_names) {
      const auto* pref = local_state_.FindPreference(pref_name);
      ASSERT_TRUE(pref) << pref_name;
      const uint32_t flags = pref->registration_flags();
      bool is_syncable =
          (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF) != 0;
      EXPECT_EQ(expect_sync, is_syncable);
    }
  }
  const AccountId affiliated_user_account_ =
      AccountId::FromUserEmailGaiaId("user1@gmail.com", GaiaId("fakegaia1"));
  const AccountId unaffiliated_user_account_ =
      AccountId::FromUserEmailGaiaId("user2@gmail.com", GaiaId("fakegaia2"));
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  raw_ptr<const user_manager::User> affiliated_user_;
  raw_ptr<const user_manager::User> unaffiliated_user_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
};

TEST_F(BocaRoleUtilTest, TestCheckPrefsSyncableFlags) {
  constexpr auto kSyncablePrefs = std::to_array<const char*>(
      {ash::prefs::kClassManagementToolsOOBEAccessCountSetting,
       ash::prefs::kClassManagementToolsKioskReceiverCodes});
  CheckPrefsSyncableFlags(kSyncablePrefs, /*expect_sync=*/true);

  constexpr auto kNonSyncablePrefs = std::to_array<const char*>(
      {ash::prefs::kClassManagementToolsAvailabilitySetting,
       ash::prefs::kClassManagementToolsNavRuleSetting,
       ash::prefs::kClassManagementToolsCaptionEnablementSetting,
       ash::prefs::kClassManagementToolsClassroomEligibilitySetting,
       ash::prefs::kClassManagementToolsViewScreenEligibilitySetting,
       ash::prefs::kClassManagementToolsNetworkRestrictionSetting});
  CheckPrefsSyncableFlags(kNonSyncablePrefs, /*expect_sync=*/false);
}

TEST_F(BocaRoleUtilTest, TestDisabledForUnAffliatedUser) {
  user_manager_->OnUserProfileCreated(unaffiliated_user_account_,
                                      &local_state_);
  EXPECT_FALSE(ash::boca_util::IsEnabled(unaffiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaDisabledFromPref) {
  user_manager_->OnUserProfileCreated(affiliated_user_account_, &local_state_);
  local_state_.SetString(ash::prefs::kClassManagementToolsAvailabilitySetting,
                         "disabled");
  EXPECT_FALSE(ash::boca_util::IsEnabled(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaDisabledByDefaultFromPref) {
  user_manager_->OnUserProfileCreated(affiliated_user_account_, &local_state_);
  local_state_.SetString(ash::prefs::kClassManagementToolsAvailabilitySetting,
                         "");
  EXPECT_FALSE(ash::boca_util::IsEnabled(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaSetTeacherFromPref) {
  user_manager_->OnUserProfileCreated(affiliated_user_account_, &local_state_);
  local_state_.SetString(ash::prefs::kClassManagementToolsAvailabilitySetting,
                         "teacher");
  EXPECT_TRUE(ash::boca_util::IsEnabled(affiliated_user_));
  EXPECT_TRUE(ash::boca_util::IsProducer(affiliated_user_));
  EXPECT_FALSE(ash::boca_util::IsConsumer(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaSetStudentFromPref) {
  user_manager_->OnUserProfileCreated(affiliated_user_account_, &local_state_);
  local_state_.SetString(ash::prefs::kClassManagementToolsAvailabilitySetting,
                         "student");
  EXPECT_TRUE(ash::boca_util::IsEnabled(affiliated_user_));
  EXPECT_FALSE(ash::boca_util::IsProducer(affiliated_user_));
  EXPECT_TRUE(ash::boca_util::IsConsumer(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaEnableStudentFromFeatureFlag) {
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kBoca, ash::features::kBocaConsumer},
      /*disabled_features=*/{});
  EXPECT_TRUE(ash::boca_util::IsEnabled(nullptr));

  EXPECT_TRUE(ash::boca_util::IsConsumer(nullptr));
}

TEST_F(BocaRoleUtilTest, TestBocaEnableTeacherFromFeatureFlag) {
  scoped_feature_list_.InitWithFeatures({ash::features::kBoca},
                                        /*disabled_features=*/{});
  EXPECT_TRUE(ash::boca_util::IsEnabled(nullptr));
  EXPECT_TRUE(ash::boca_util::IsProducer(nullptr));
}

}  // namespace
