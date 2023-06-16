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
    base::test::ScopedFeatureList* scoped_feature_list) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  if (crosapi_state == web_app::test::CrosapiParam::kEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    enabled_features.push_back(features::kWebAppsCrosapi);
    enabled_features.push_back(ash::features::kLacrosSupport);
    enabled_features.push_back(ash::features::kLacrosPrimary);
    enabled_features.push_back(ash::features::kLacrosOnly);
    // Disable profile migration to avoid potential Ash restart.
    enabled_features.push_back(ash::features::kLacrosProfileMigrationForceOff);
#else
    NOTREACHED();
#endif
  } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    disabled_features.push_back(features::kWebAppsCrosapi);
    disabled_features.push_back(ash::features::kLacrosSupport);
    disabled_features.push_back(ash::features::kLacrosPrimary);
    disabled_features.push_back(ash::features::kLacrosOnly);
#endif
  }
  scoped_feature_list->InitWithFeatures(enabled_features, disabled_features);
}
