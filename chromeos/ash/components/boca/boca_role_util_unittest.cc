// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_role_util.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id_literal.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr AccountId::Literal kAffiliatedUserAccount =
    AccountId::Literal::FromUserEmailGaiaId("user1@gmail.com",
                                            GaiaId::Literal("fakegaia1"));
constexpr AccountId::Literal kUnaffiliatedUserAccount =
    AccountId::Literal::FromUserEmailGaiaId("user2@gmail.com",
                                            GaiaId::Literal("fakegaia2"));

template <size_t N>
void CheckPrefsSyncableFlags(const PrefService& prefs,
                             const std::array<const char*, N>& pref_names,
                             bool expect_sync) {
  for (const char* pref_name : pref_names) {
    const auto* pref = prefs.FindPreference(pref_name);
    ASSERT_TRUE(pref) << pref_name;
    const uint32_t flags = pref->registration_flags();
    bool is_syncable =
        (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF) != 0;
    EXPECT_EQ(expect_sync, is_syncable);
  }
}

class BocaRoleUtilTest : public testing::Test {
 public:
  BocaRoleUtilTest() = default;
  ~BocaRoleUtilTest() override = default;

  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    ash::boca_util::RegisterPrefs(affiliated_user_prefs_.registry());
    ash::boca_util::RegisterPrefs(unaffiliated_user_prefs_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
    affiliated_user_ =
        user_session_test_environment_->AddRegularUser(kAffiliatedUserAccount);
    ASSERT_TRUE(affiliated_user_);
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        kAffiliatedUserAccount,
        /*is_managed=*/true,
        /*is_affiliated=*/true);
    unaffiliated_user_ = user_session_test_environment_->AddRegularUser(
        kUnaffiliatedUserAccount);
    ASSERT_TRUE(unaffiliated_user_);
    user_manager::UserManager::Get()->OnUserProfileCreated(
        kAffiliatedUserAccount, &affiliated_user_prefs_);
    user_manager::UserManager::Get()->OnUserProfileCreated(
        kUnaffiliatedUserAccount, &unaffiliated_user_prefs_);
  }

  void TearDown() override {
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
        kUnaffiliatedUserAccount);
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
        kAffiliatedUserAccount);
    affiliated_user_ = nullptr;
    unaffiliated_user_ = nullptr;
    user_session_test_environment_.reset();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  // Profile prefs, one per user so values cannot leak between them.
  TestingPrefServiceSimple affiliated_user_prefs_;
  TestingPrefServiceSimple unaffiliated_user_prefs_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<const user_manager::User> affiliated_user_ = nullptr;
  raw_ptr<const user_manager::User> unaffiliated_user_ = nullptr;
};

TEST_F(BocaRoleUtilTest, TestCheckPrefsSyncableFlags) {
  TestingPrefServiceSimple prefs;
  ash::boca_util::RegisterPrefs(prefs.registry());

  constexpr auto kSyncablePrefs = std::to_array<const char*>(
      {ash::prefs::kClassManagementToolsOOBEAccessCountSetting,
       ash::prefs::kClassManagementToolsKioskReceiverCodes});
  CheckPrefsSyncableFlags(prefs, kSyncablePrefs, /*expect_sync=*/true);

  constexpr auto kNonSyncablePrefs = std::to_array<const char*>(
      {ash::prefs::kClassManagementToolsAvailabilitySetting,
       ash::prefs::kClassManagementToolsNavRuleSetting,
       ash::prefs::kClassManagementToolsCaptionEnablementSetting,
       ash::prefs::kClassManagementToolsClassroomEligibilitySetting,
       ash::prefs::kClassManagementToolsViewScreenEligibilitySetting,
       ash::prefs::kClassManagementToolsNetworkRestrictionSetting});
  CheckPrefsSyncableFlags(prefs, kNonSyncablePrefs, /*expect_sync=*/false);
}

TEST_F(BocaRoleUtilTest, TestDisabledForUnAffliatedUser) {
  EXPECT_FALSE(ash::boca_util::IsEnabled(unaffiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaDisabledFromPref) {
  affiliated_user_prefs_.SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "disabled");
  EXPECT_FALSE(ash::boca_util::IsEnabled(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaDisabledByDefaultFromPref) {
  affiliated_user_prefs_.SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "");
  EXPECT_FALSE(ash::boca_util::IsEnabled(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaSetTeacherFromPref) {
  affiliated_user_prefs_.SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "teacher");
  EXPECT_TRUE(ash::boca_util::IsEnabled(affiliated_user_));
  EXPECT_TRUE(ash::boca_util::IsProducer(affiliated_user_));
  EXPECT_FALSE(ash::boca_util::IsConsumer(affiliated_user_));
}

TEST_F(BocaRoleUtilTest, TestBocaSetStudentFromPref) {
  affiliated_user_prefs_.SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "student");
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
