// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"

#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using testing::IsFalse;
using testing::IsTrue;

class IsolatedWebAppFeaturesTest : public WebAppTest {
 protected:
  void SetDeveloperToolsAvailabilityPolicy(
      policy::DeveloperToolsPolicyHandler::Availability availability) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kDevToolsAvailability,
        base::Value(base::to_underlying(availability)));
  }
};

TEST_F(IsolatedWebAppFeaturesTest, IwaAndDevModeEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kIsolatedWebAppDevMode},
                                       {features::kIsolatedWebApps});
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaEnabledDevModeDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kIsolatedWebApps},
                                       {features::kIsolatedWebAppDevMode});
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaEnabledDevPolicyDisabled) {
  base::test::ScopedFeatureList scoped_feature_list{features::kIsolatedWebApps};

  SetDeveloperToolsAvailabilityPolicy(
      policy::DeveloperToolsPolicyHandler::Availability::
          kDisallowedForForceInstalledExtensions);
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaEnabledDevModeEnabledDevPolicyDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsTrue());

  SetDeveloperToolsAvailabilityPolicy(
      policy::DeveloperToolsPolicyHandler::Availability::kDisallowed);
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaDisabledUnmanagedInstallDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {},
      {features::kIsolatedWebApps, features::kIsolatedWebAppUnmanagedInstall});
  EXPECT_THAT(IsIwaUnmanagedInstallEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaDisabledUnmanagedInstallEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatedWebAppUnmanagedInstall},
      {features::kIsolatedWebApps});
  EXPECT_THAT(IsIwaUnmanagedInstallEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaEnabledUnmanagedInstallDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatedWebApps},
      {features::kIsolatedWebAppUnmanagedInstall});
  EXPECT_THAT(IsIwaUnmanagedInstallEnabled(profile()), IsFalse());
}

TEST_F(IsolatedWebAppFeaturesTest, IwaEnabledUnmanagedInstallEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kIsolatedWebApps, features::kIsolatedWebAppUnmanagedInstall},
      {});
  EXPECT_THAT(IsIwaUnmanagedInstallEnabled(profile()), IsTrue());
}

}  // namespace
}  // namespace web_app
