// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/profile_test_helper.h"

#include <vector>

#include "base/notreached.h"
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#endif

std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileParam>& info) {
  std::string result;
  switch (info.param.profile_type) {
    case TestProfileType::kRegular:
      result = "Regular";
      break;
    case TestProfileType::kIncognito:
      result = "Incognito";
      break;
    case TestProfileType::kGuest:
      result = "Guest";
      break;
  }

  if (info.param.crosapi_state == web_app::test::CrosapiParam::kEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    result += "_Crosapi";
#else
    NOTREACHED();
#endif
  }

  switch (info.param.external_pref_migration_case) {
    case ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
      result += "_DisableMigration_ReadFromPrefs";
      break;
    case ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
      result += "_DisableMigration_ReadFromDB";
      break;
    case ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
      result += "EnableMigration_ReadFromPrefs";
      break;
    case ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
      result += "EnableMigration_ReadFromDB";
      break;
  }

  return result;
}

void ConfigureCommandLineForGuestMode(base::CommandLine* command_line) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line->AppendSwitch(ash::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "hash");
  command_line->AppendSwitchASCII(
      ash::switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
#else
  NOTREACHED();
#endif
}

void InitCrosapiFeaturesForParam(
    web_app::test::CrosapiParam crosapi_state,
    base::test::ScopedFeatureList* scoped_feature_list,
    ExternalPrefMigrationTestCases external_pref_migration_case) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  switch (external_pref_migration_case) {
    case ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
      disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
      disabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
      break;
    case ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
      disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
      enabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
      break;
    case ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
      enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
      disabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
      break;
    case ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
      enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
      enabled_features.push_back(features::kUseWebAppDBInsteadOfExternalPrefs);
      break;
  }

  if (crosapi_state == web_app::test::CrosapiParam::kEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    enabled_features.push_back(features::kWebAppsCrosapi);
#else
    NOTREACHED();
#endif
  } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    disabled_features.push_back(features::kWebAppsCrosapi);
    disabled_features.push_back(ash::features::kLacrosPrimary);
#endif
  }
  scoped_feature_list->InitWithFeatures(enabled_features, disabled_features);
}
