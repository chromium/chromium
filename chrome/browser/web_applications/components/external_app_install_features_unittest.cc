// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_app_install_features.h"

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

class ExternalWebAppInstallFeaturesTest
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

  ExternalWebAppInstallFeaturesTest() {
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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    if (MigrateManaged()) {
      managed_migration_.InitAndEnableFeature(
          kAllowDefaultWebAppMigrationForChromeOsManagedUsers);
    } else {
      managed_migration_.InitAndDisableFeature(
          kAllowDefaultWebAppMigrationForChromeOsManagedUsers);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  ~ExternalWebAppInstallFeaturesTest() override = default;

  bool MigrateBase() const { return GetParam().base; }
  bool MigrateManaged() const { return GetParam().managed; }

  void ExpectMigrationEnabled(bool unmanaged_expectation,
                              bool managed_expectation) {
    {
      TestingProfile::Builder builder;
      builder.OverridePolicyConnectorIsManagedForTesting(false);
      std::unique_ptr<TestingProfile> unmanaged_profile = builder.Build();
      EXPECT_EQ(
          IsExternalAppInstallFeatureEnabled(
              kMigrateDefaultChromeAppToWebAppsGSuite.name, *unmanaged_profile),
          unmanaged_expectation);
      EXPECT_EQ(IsExternalAppInstallFeatureEnabled(
                    kMigrateDefaultChromeAppToWebAppsNonGSuite.name,
                    *unmanaged_profile),
                unmanaged_expectation);
    }

    {
      TestingProfile::Builder builder;
      builder.OverridePolicyConnectorIsManagedForTesting(true);
      std::unique_ptr<TestingProfile> managed_profile = builder.Build();
      EXPECT_EQ(
          IsExternalAppInstallFeatureEnabled(
              kMigrateDefaultChromeAppToWebAppsGSuite.name, *managed_profile),
          managed_expectation);
      EXPECT_EQ(IsExternalAppInstallFeatureEnabled(
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

TEST_P(ExternalWebAppInstallFeaturesTest, MigrationEnabled) {
  ExpectMigrationEnabled(
      /*unmanaged_expectation=*/MigrateBase(),
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      /*managed_expectation=*/MigrateBase() && MigrateManaged()
#else
      /*managed_expectation=*/MigrateBase()
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  );
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternalWebAppInstallFeaturesTest,
    testing::Values(Migrate{.base = false, .managed = false},
                    Migrate{.base = false, .managed = true},
                    Migrate{.base = true, .managed = false},
                    Migrate{.base = true, .managed = true}),
    &ExternalWebAppInstallFeaturesTest::ParamToString);

}  // namespace web_app
