// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

constexpr char kTestAppId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kTestAppId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

std::unique_ptr<ScopedTestingPreinstalledAppData> CreateStubPreinstalledApps() {
  auto app_data = std::make_unique<ScopedTestingPreinstalledAppData>();
  app_data->apps.push_back({GURL("https://one.example"),
                            kMigrateDefaultChromeAppToWebAppsGSuite.name,
                            kTestAppId1});
  app_data->apps.push_back({GURL("https://two.example"),
                            kMigrateDefaultChromeAppToWebAppsNonGSuite.name,
                            kTestAppId2});
  return app_data;
}

}  // namespace

using PreinstalledWebAppsTest = testing::Test;

TEST(PreinstalledWebAppsTest, AppsOnlyReturnedIfSpecificFeatureEnabled) {
  auto scoped_preinstalled_apps = CreateStubPreinstalledApps();

  // The preinstalled apps depend on two different features (in addition to
  // default web app installation).
  {
    // With both features disabled, no apps are included.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kDefaultWebAppInstallation},
                                  {kMigrateDefaultChromeAppToWebAppsGSuite,
                                   kMigrateDefaultChromeAppToWebAppsNonGSuite});
    PreinstalledWebApps preinstalled_web_apps = GetPreinstalledWebApps();
    EXPECT_EQ(2, preinstalled_web_apps.disabled_count);
    EXPECT_EQ(0u, preinstalled_web_apps.options.size());
  }

  {
    // Enable a single feature; only the corresponding app should be returned.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kDefaultWebAppInstallation,
                                   kMigrateDefaultChromeAppToWebAppsGSuite},
                                  {kMigrateDefaultChromeAppToWebAppsNonGSuite});
    PreinstalledWebApps preinstalled_web_apps = GetPreinstalledWebApps();
    EXPECT_EQ(1, preinstalled_web_apps.disabled_count);
    ASSERT_EQ(1u, preinstalled_web_apps.options.size());
    const ExternalInstallOptions& options = preinstalled_web_apps.options[0];
    ASSERT_EQ(1u, options.uninstall_and_replace.size());
    EXPECT_EQ(kTestAppId1, options.uninstall_and_replace[0]);
  }

  {
    // Enable the second feature; the corresponding app should be returned.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kDefaultWebAppInstallation,
                                   kMigrateDefaultChromeAppToWebAppsNonGSuite},
                                  {kMigrateDefaultChromeAppToWebAppsGSuite});
    PreinstalledWebApps preinstalled_web_apps = GetPreinstalledWebApps();
    EXPECT_EQ(1, preinstalled_web_apps.disabled_count);
    ASSERT_EQ(1u, preinstalled_web_apps.options.size());
    const ExternalInstallOptions& options = preinstalled_web_apps.options[0];
    ASSERT_EQ(1u, options.uninstall_and_replace.size());
    EXPECT_EQ(kTestAppId2, options.uninstall_and_replace[0]);
  }
}

}  // namespace web_app
