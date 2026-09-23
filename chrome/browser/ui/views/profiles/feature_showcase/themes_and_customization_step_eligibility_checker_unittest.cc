// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/themes_and_customization_step_eligibility_checker.h"

#include "base/test/test_future.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/themes/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ThemesAndCustomizationStepEligibilityCheckerTest : public testing::Test {
 public:
  ThemesAndCustomizationStepEligibilityCheckerTest() = default;
  ~ThemesAndCustomizationStepEligibilityCheckerTest() override = default;

 protected:
  TestingProfile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ThemesAndCustomizationStepEligibilityCheckerTest,
       EligibleWhenNoPolicyTheme) {
  ThemesAndCustomizationStepEligibilityChecker checker;

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ThemesAndCustomizationStepEligibilityCheckerTest,
       IneligibleWhenPolicyTheme) {
  ThemesAndCustomizationStepEligibilityChecker checker;

  profile().GetTestingPrefService()->SetManagedPref(themes::kPolicyThemeColor,
                                                    base::Value(123456));

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
