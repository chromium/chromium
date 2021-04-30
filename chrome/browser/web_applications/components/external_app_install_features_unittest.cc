// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_app_install_features.h"

#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif

namespace web_app {

struct Migrate {
  bool beta;
};

class ExternalWebAppInstallFeaturesTest
    : public testing::Test,
      public ::testing::WithParamInterface<Migrate> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<Migrate> param_info) {
    return param_info.param.beta ? "MigrateBeta" : "UnmigrateBeta";
  }

  ExternalWebAppInstallFeaturesTest() {
    base_migration_.InitWithFeatures(
        {kMigrateDefaultChromeAppToWebAppsGSuite,
         kMigrateDefaultChromeAppToWebAppsNonGSuite},
        {});
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (GetMigrate().beta) {
      beta_migration_.InitAndEnableFeature(
          kMigrateDefaultChromeAppToWebAppsChromeOsBeta);
    } else {
      beta_migration_.InitAndDisableFeature(
          kMigrateDefaultChromeAppToWebAppsChromeOsBeta);
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  }
  ~ExternalWebAppInstallFeaturesTest() override = default;

  const Migrate& GetMigrate() { return GetParam(); }

 private:
  base::test::ScopedFeatureList base_migration_;
  base::test::ScopedFeatureList beta_migration_;
};

TEST_P(ExternalWebAppInstallFeaturesTest, NonBetaChannel) {
  EXPECT_TRUE(IsExternalAppInstallFeatureEnabled(
      kMigrateDefaultChromeAppToWebAppsGSuite.name));
  EXPECT_TRUE(IsExternalAppInstallFeatureEnabled(
      kMigrateDefaultChromeAppToWebAppsNonGSuite.name));
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_P(ExternalWebAppInstallFeaturesTest, BetaChannel) {
  base::SysInfo::SetChromeOSVersionInfoForTest(
      base::StrCat(
          {crosapi::kChromeOSReleaseTrack, "=", crosapi::kReleaseChannelBeta}),
      base::Time::Now());

  EXPECT_EQ(IsExternalAppInstallFeatureEnabled(
                kMigrateDefaultChromeAppToWebAppsGSuite.name),
            GetMigrate().beta);
  EXPECT_EQ(IsExternalAppInstallFeatureEnabled(
                kMigrateDefaultChromeAppToWebAppsNonGSuite.name),
            GetMigrate().beta);

  base::SysInfo::ResetChromeOSVersionInfoForTest();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

INSTANTIATE_TEST_SUITE_P(All,
                         ExternalWebAppInstallFeaturesTest,
                         testing::Values(Migrate{.beta = true},
                                         Migrate{.beta = false}),
                         &ExternalWebAppInstallFeaturesTest::ParamToString);

}  // namespace web_app
