// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using testing::IsFalse;
using testing::IsTrue;

TEST(IsolatedWebAppDevModeTest, IsIwaDevModeEnabled) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(
      policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed, true);

  EXPECT_THAT(IsIwaDevModeEnabled(pref_service), IsFalse());

  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kIsolatedWebAppDevMode};
    // `features::kIsolatedWebApps` is not enabled.
    EXPECT_THAT(IsIwaDevModeEnabled(pref_service), IsFalse());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kIsolatedWebApps};
    // `features::kIsolatedWebAppDevMode` is not enabled.
    EXPECT_THAT(IsIwaDevModeEnabled(pref_service), IsFalse());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    EXPECT_THAT(IsIwaDevModeEnabled(pref_service), IsTrue());

    pref_service.SetManagedPref(
        policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed,
        base::Value(false));
    EXPECT_THAT(IsIwaDevModeEnabled(pref_service), IsFalse());
  }
}

}  // namespace
}  // namespace web_app
