// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
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

using IsolatedWebAppDevModeTest = WebAppTest;

TEST_F(IsolatedWebAppDevModeTest, IsIwaDevModeEnabled) {
  profile()->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed,
      base::Value(true));
  EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());

  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kIsolatedWebAppDevMode};
    // `features::kIsolatedWebApps` is not enabled.
    EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kIsolatedWebApps};
    // `features::kIsolatedWebAppDevMode` is not enabled.
    EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsTrue());

    profile()->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed,
        base::Value(false));
    EXPECT_THAT(IsIwaDevModeEnabled(profile()), IsFalse());
  }
}

}  // namespace
}  // namespace web_app
