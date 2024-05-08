// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"

#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_trial.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultBrowserPromptBrowserTest : public InProcessBrowserTest {
public:
  static constexpr char kStudyTestGroupName[] = "test_group_1";

  void SetUp() override {
    InitFeaturesWithStudyGroup(kStudyTestGroupName);
    InProcessBrowserTest::SetUp();
  }

  void InitFeaturesWithStudyGroup(std::string group_name) {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{features::kDefaultBrowserPromptRefresh, {{}}},
         {features::kDefaultBrowserPromptRefreshTrial,
          {{"group_name", group_name}}}},
        {{}});
  }

  PrefService *local_state() { return g_browser_process->local_state(); }

private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptBrowserTest,
                       JoinsSyntheticTrialCohort) {
  DefaultBrowserPromptTrial::MaybeJoinDefaultBrowserPromptCohort();

  EXPECT_EQ(
      kStudyTestGroupName,
      local_state()->GetString(prefs::kDefaultBrowserPromptRefreshStudyGroup));
  EXPECT_TRUE(
      variations::HasSyntheticTrial("DefaultBrowserPromptRefreshSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "DefaultBrowserPromptRefreshSynthetic", kStudyTestGroupName));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptBrowserTest,
                       StickingToCohortDoesNotActivateExperiment) {
  local_state()->SetString(prefs::kDefaultBrowserPromptRefreshStudyGroup,
                           kStudyTestGroupName);
  DefaultBrowserPromptTrial::EnsureStickToDefaultBrowserPromptCohort();

  EXPECT_TRUE(
      variations::HasSyntheticTrial("DefaultBrowserPromptRefreshSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "DefaultBrowserPromptRefreshSynthetic", kStudyTestGroupName));
}

class DefaultBrowserPromptDisabledBrowserTest
    : public DefaultBrowserPromptBrowserTest {
  void SetUp() override {
    InitFeaturesWithStudyGroup("");
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptDisabledBrowserTest,
                       DoesNotJoinSyntheticTrialCohort) {
  DefaultBrowserPromptTrial::MaybeJoinDefaultBrowserPromptCohort();

  EXPECT_EQ("", local_state()->GetString(
                    prefs::kDefaultBrowserPromptRefreshStudyGroup));
  EXPECT_FALSE(
      variations::HasSyntheticTrial("DefaultBrowserPromptRefreshSynthetic"));
  EXPECT_FALSE(variations::IsInSyntheticTrialGroup(
      "DefaultBrowserPromptRefreshSynthetic", kStudyTestGroupName));
}
