// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"

#include "base/android/build_info.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr std::string kGmsVersionWithoutLocalPasswordsSupport = "223012000";
constexpr std::string kGmsVersionWithLocalPasswordsSupport = "240212000";

class SplitStoresAndLocalUpmTest : public ::testing::Test {
 public:
  SplitStoresAndLocalUpmTest() {
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }

  PrefService* pref_service() { return &pref_service_; }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOff) {
  EXPECT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOffAndMigrationPending) {
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));

  EXPECT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOn) {
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

struct IsGmsCoreUpdateRequiredTestCase {
  std::string test_case_desc;
  std::string gms_version;
  bool is_pwd_sync_enabled;
  bool is_evicted;
  bool was_initial_migration_done;
  bool sync_only_in_gms_enabled;
  bool is_login_db_empty;
  bool expected_is_update_required_automotive;
  bool expected_is_update_required;
};

class SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired
    : public SplitStoresAndLocalUpmTest,
      public ::testing::WithParamInterface<IsGmsCoreUpdateRequiredTestCase> {
 public:
  SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        false);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
        0);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kEmptyProfileStoreLoginDatabase, false);
  }
};

TEST_P(SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired,
       IsGmsCoreUpdateRequired) {
  IsGmsCoreUpdateRequiredTestCase p = GetParam();
  base::test::ScopedFeatureList features;
  if (p.sync_only_in_gms_enabled) {
    features.InitAndEnableFeature(
        password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore);
  } else {
    features.InitAndDisableFeature(
        password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore);
  }

  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      p.is_evicted);
  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      p.was_initial_migration_done);
  pref_service()->SetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase,
      p.is_login_db_empty);

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  bool expected_is_update_required =
      base::android::BuildInfo::GetInstance()->is_automotive()
          ? p.expected_is_update_required_automotive
          : p.expected_is_update_required;
  EXPECT_EQ(expected_is_update_required,
            IsGmsCoreUpdateRequired(pref_service(), p.is_pwd_sync_enabled,
                                    p.gms_version));
#else
  EXPECT_EQ(false, IsGmsCoreUpdateRequired(
                       pref_service(), p.is_pwd_sync_enabled, p.gms_version));
#endif
}

INSTANTIATE_TEST_SUITE_P(
    SplitStoresAndLocalUpmTest,
    SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired,
    testing::Values(
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "FalseWhenFeatureDisabled",
            .gms_version = "1",
            .is_pwd_sync_enabled = true,
            .is_evicted = false,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = false,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = false,
            .expected_is_update_required = false},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "TrueForVeryOldGms",
            .gms_version = "1",
            .is_pwd_sync_enabled = true,
            .is_evicted = false,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = true},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "TrueForNonSyncingGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = false,
            .is_evicted = false,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = true},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "TrueForEvictedUsersGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = true,
            .is_evicted = true,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = true},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "TrueForNotEnrolledGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = true,
            .is_evicted = false,
            .was_initial_migration_done = false,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = true},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "FalseForNewGms",
            .gms_version = kGmsVersionWithLocalPasswordsSupport,
            .is_pwd_sync_enabled = false,
            .is_evicted = false,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = false},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "FalseForSyncingGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = true,
            .is_evicted = false,
            .was_initial_migration_done = true,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = false,
            .expected_is_update_required_automotive = false,
            .expected_is_update_required = false},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc =
                "FalseForNotEnrolledWithEmptyLoginDBGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = true,
            .is_evicted = true,
            .was_initial_migration_done = false,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = true,
            .expected_is_update_required_automotive = false,
            .expected_is_update_required = false},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc =
                "FalseForNotMigratedWithEmptyLoginDBGmsDoesNotSupportLocal",
            .gms_version = kGmsVersionWithoutLocalPasswordsSupport,
            .is_pwd_sync_enabled = true,
            .is_evicted = false,
            .was_initial_migration_done = false,
            .sync_only_in_gms_enabled = true,
            .is_login_db_empty = true,
            .expected_is_update_required_automotive = false,
            .expected_is_update_required = false}),
    [](const ::testing::TestParamInfo<IsGmsCoreUpdateRequiredTestCase>& info) {
      return info.param.test_case_desc;
    });

}  // namespace
}  // namespace password_manager
