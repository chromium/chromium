// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/profile_test_helper.h"

#include <vector>

#include "base/containers/extend.h"
#include "base/notreached.h"
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi_state == web_app::test::CrosapiParam::kEnabled) {
    base::Extend(enabled_features, ash::standalone_browser::GetFeatureRefs());
  } else {
    base::Extend(disabled_features, ash::standalone_browser::GetFeatureRefs());
  }
#else
    NOTREACHED();
#endif
  scoped_feature_list->InitWithFeatures(enabled_features, disabled_features);
}
