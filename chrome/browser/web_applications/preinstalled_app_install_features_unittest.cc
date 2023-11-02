// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_app_install_features.h"

#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

struct Migrate {
  bool base;
  bool managed;
};

class PreinstalledWebAppInstallFeaturesTest
    : public testing::Test,
      public ::testing::WithParamInterface<Migrate> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<Migrate> param_info) {
    std::string result = "Migrate";
    if (param_info.param.base)
      result += "Base";
    if (param_info.param.managed)
      result += "Managed";
    if (result == "Migrate")
      result += "None";
    return result;
  }

  PreinstalledWebAppInstallFeaturesTest() {
    if (MigrateBase()) {
      base_migration_.InitWithFeatures(
          {kMigrateDefaultChromeAppToWebAppsGSuite,
           kMigrateDefaultChromeAppToWebAppsNonGSuite},
          {});
    } else {
      base_migration_.InitWithFeatures(
          {}, {kMigrateDefaultChromeAppToWebAppsGSuite,
               kMigrateDefaultChromeAppToWebAppsNonGSuite});
    }
#if BUILDFLAG(IS_CHROMEOS)
    if (MigrateManaged()) {
      managed_migration_.InitAndEnableFeature(
          kAllowDefaultWebAppMigrationForChromeOsManagedUsers);
    } else {
      managed_migration_.InitAndDisableFeature(
          kAllowDefaultWebAppMigrationForChromeOsManagedUsers);
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  ~PreinstalledWebAppInstallFeaturesTest() override = default;

  bool MigrateBase() const { return GetParam().base; }
  bool MigrateManaged() const { return GetParam().managed; }

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
      /*unmanaged_expectation=*/MigrateBase(),
#if BUILDFLAG(IS_CHROMEOS)
      /*managed_expectation=*/MigrateBase() && MigrateManaged()
#else
      /*managed_expectation=*/MigrateBase()
#endif  // BUILDFLAG(IS_CHROMEOS)
  );
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PreinstalledWebAppInstallFeaturesTest,
    testing::Values(Migrate{.base = false, .managed = false},
                    Migrate{.base = false, .managed = true},
                    Migrate{.base = true, .managed = false},
                    Migrate{.base = true, .managed = true}),
    &PreinstalledWebAppInstallFeaturesTest::ParamToString);

}  // namespace web_app
