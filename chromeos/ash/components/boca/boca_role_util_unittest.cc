// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_role_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
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
    affiliated_user_ = user_manager_->AddUserWithAffiliation(
        affiliated_user_account_, /*is_affiliated=*/true);
    unaffiliated_user_ = user_manager_->AddUserWithAffiliation(
        unaffiliated_user_account_, /*is_affiliated=*/false);
  }

  void TearDown() override {
    affiliated_user_ = nullptr;
    unaffiliated_user_ = nullptr;
    user_manager_->Destroy();
    user_manager_.reset();
  }

 protected:
  const AccountId affiliated_user_account_ =
      AccountId::FromUserEmail("user1@gmail.com");
  const AccountId unaffiliated_user_account_ =
      AccountId::FromUserEmail("user2@gmail.com");
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  raw_ptr<const user_manager::User> affiliated_user_;
  raw_ptr<const user_manager::User> unaffiliated_user_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
};

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
