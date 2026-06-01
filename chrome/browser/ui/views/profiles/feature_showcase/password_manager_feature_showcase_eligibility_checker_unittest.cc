// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/password_manager_feature_showcase_eligibility_checker.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace {

class PasswordManagerFeatureShowcaseEligibilityCheckerTest
    : public testing::Test {
 public:
  PasswordManagerFeatureShowcaseEligibilityCheckerTest() = default;
  ~PasswordManagerFeatureShowcaseEligibilityCheckerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    InitializeActionIdStringMapping();
  }

  void TearDown() override {
    actions::ActionIdMap::ResetMapsForTesting();
    testing::Test::TearDown();
  }

 protected:
  TestingProfile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(PasswordManagerFeatureShowcaseEligibilityCheckerTest,
       EligibleWhenEnabledAndNotPinned) {
  PasswordManagerFeatureShowcaseEligibilityChecker checker;
  profile().GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(PasswordManagerFeatureShowcaseEligibilityCheckerTest,
       IneligibleWhenPinned) {
  PasswordManagerFeatureShowcaseEligibilityChecker checker;
  profile().GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, true);

  PinnedToolbarActionsModel* model = PinnedToolbarActionsModel::Get(&profile());
  ASSERT_TRUE(model);
  model->UpdatePinnedState(kActionShowPasswordsBubbleOrPage, true);

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerFeatureShowcaseEligibilityCheckerTest,
       IneligibleWhenDisabled) {
  PasswordManagerFeatureShowcaseEligibilityChecker checker;
  profile().GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  PinnedToolbarActionsModel* model = PinnedToolbarActionsModel::Get(&profile());
  ASSERT_TRUE(model);

  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
