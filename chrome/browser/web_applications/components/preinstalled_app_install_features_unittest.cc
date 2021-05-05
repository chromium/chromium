// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/preinstalled_app_install_features.h"

#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

struct Migrate {
  bool managed;
};

class PreinstalledWebAppInstallFeaturesTest
    : public testing::Test,
      public ::testing::WithParamInterface<Migrate> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<Migrate> param_info) {
    return param_info.param.managed ? "MigrateManaged" : "UnmigrateManaged";
  }

  PreinstalledWebAppInstallFeaturesTest() {
    base_migration_.InitWithFeatures(
        {kMigrateDefaultChromeAppToWebAppsGSuite,
         kMigrateDefaultChromeAppToWebAppsNonGSuite},
        {});
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    if (GetMigrate().managed) {
      managed_migration_.InitAndEnableFeature(
          kMigrateDefaultChromeAppToWebAppsChromeOsManaged);
    } else {
      managed_migration_.InitAndDisableFeature(
          kMigrateDefaultChromeAppToWebAppsChromeOsManaged);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  ~PreinstalledWebAppInstallFeaturesTest() override = default;

  const Migrate& GetMigrate() { return GetParam(); }

  void ExpectMigrationEnabled(bool unmanaged_expectation,
                              bool managed_expectation) {
    {
      TestingProfile::Builder builder;
      builder.OverridePolicyConnectorIsManagedForTesting(false);
      std::unique_ptr<TestingProfile> unmanaged_profile = builder.Build();
      EXPECT_EQ(
          IsPreinstalledAppInstallFeatureEnabled(
              kMigrateDefaultChromeAppToWebAppsGSuite.name, *unmanaged_profile),
          unmanaged_expectation);
      EXPECT_EQ(IsPreinstalledAppInstallFeatureEnabled(
                    kMigrateDefaultChromeAppToWebAppsNonGSuite.name,
                    *unmanaged_profile),
                unmanaged_expectation);
    }

    {
      TestingProfile::Builder builder;
      builder.OverridePolicyConnectorIsManagedForTesting(true);
      std::unique_ptr<TestingProfile> managed_profile = builder.Build();
      EXPECT_EQ(
          IsPreinstalledAppInstallFeatureEnabled(
              kMigrateDefaultChromeAppToWebAppsGSuite.name, *managed_profile),
          managed_expectation);
      EXPECT_EQ(IsPreinstalledAppInstallFeatureEnabled(
                    kMigrateDefaultChromeAppToWebAppsNonGSuite.name,
                    *managed_profile),
                managed_expectation);
    }
  }

 protected:
  base::test::ScopedFeatureList base_migration_;
  base::test::ScopedFeatureList managed_migration_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(PreinstalledWebAppInstallFeaturesTest, MigrationEnabled) {
  ExpectMigrationEnabled(
      /*unmanaged_expectation=*/true,
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      /*managed_expectation=*/GetMigrate().managed
#else
      /*managed_expectation=*/true
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  );
}

INSTANTIATE_TEST_SUITE_P(All,
                         PreinstalledWebAppInstallFeaturesTest,
                         testing::Values(Migrate{.managed = false},
                                         Migrate{.managed = true}),
                         &PreinstalledWebAppInstallFeaturesTest::ParamToString);

}  // namespace web_app
