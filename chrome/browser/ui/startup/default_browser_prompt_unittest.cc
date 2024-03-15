// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include <map>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultBrowserPromptTest : public testing::Test {
 protected:
  void SetUp() override {
    testing_profile_manager = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager->SetUp());
    testing_profile =
        testing_profile_manager->CreateTestingProfile("Test Profile");
  }

  void EnableDefaultBrowserPromptRefreshFeatureWithParams(
      std::map<std::string, std::string> params) {
    scoped_feature_list.Reset();
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kDefaultBrowserPromptRefresh, params);
  }

  void TestShouldShowDefaultBrowserPrompt(
      std::optional<base::TimeDelta> last_declined_time_delta,
      std::optional<int> declined_count,
      bool expected) {
    if (last_declined_time_delta.has_value()) {
      local_state()->SetTime(
          prefs::kDefaultBrowserLastDeclinedTime,
          base::Time::Now() - last_declined_time_delta.value());
    } else {
      local_state()->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
    }
    if (declined_count.has_value()) {
      local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount,
                                declined_count.value());
    } else {
      local_state()->ClearPref(prefs::kDefaultBrowserDeclinedCount);
    }
    EXPECT_EQ(expected, ShouldShowDefaultBrowserPromptForTesting(profile()));
  }

  PrefService* profile_prefs() { return testing_profile->GetPrefs(); }

  Profile* profile() { return testing_profile; }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<TestingProfileManager> testing_profile_manager;
  raw_ptr<TestingProfile> testing_profile;
};

TEST_F(DefaultBrowserPromptTest, MigrateLastDeclinedTimeProfilePrefDefault) {
  profile_prefs()->ClearPref(prefs::kDefaultBrowserLastDeclined);
  local_state()->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                         base::Time::Now());
  local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount, 3);

  MigrateDefaultBrowserLastDeclinedPref(profile_prefs());
  EXPECT_EQ(local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime),
            base::Time::Now());
  EXPECT_EQ(local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount), 3);
}

TEST_F(DefaultBrowserPromptTest, MigrateLastDeclinedTimeLocalPrefDefault) {
  profile_prefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                            base::Time::Now().ToInternalValue());
  local_state()->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
  local_state()->ClearPref(prefs::kDefaultBrowserDeclinedCount);

  MigrateDefaultBrowserLastDeclinedPref(profile_prefs());
  EXPECT_EQ(local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime),
            base::Time::Now());
  EXPECT_EQ(local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount), 1);
}

TEST_F(DefaultBrowserPromptTest, MigrateLastDeclinedTimeIfMoreRecent) {
  profile_prefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                            base::Time::Now().ToInternalValue());
  local_state()->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                         base::Time::Now() - base::Seconds(1));
  local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount, 1);

  MigrateDefaultBrowserLastDeclinedPref(profile_prefs());
  EXPECT_EQ(local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime),
            base::Time::Now());
  EXPECT_EQ(local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount), 1);

  profile_prefs()->SetInt64(
      prefs::kDefaultBrowserLastDeclined,
      (base::Time::Now() - base::Seconds(1)).ToInternalValue());

  MigrateDefaultBrowserLastDeclinedPref(profile_prefs());
  EXPECT_EQ(local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime),
            base::Time::Now());
  EXPECT_EQ(local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount), 1);
}

TEST_F(DefaultBrowserPromptTest, MaxPromptCount) {
  // If max prompt count is negative, do not limit the number of times the
  // prompt is shown.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/12345,
      /*expected=*/true);

  // Never show the prompt if max prompt count is zero.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "0"},
       {features::kRepromptDurationMultiplier.name, "2"}});
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/false);

  // If max prompt count is 1, only show the prompt if declined count is unset.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "1"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/true);
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/1,
      /*expected=*/false);

  // Show if the declined count is less than the max prompt count.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "5"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/4,
      /*expected=*/true);
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/5,
      /*expected=*/false);
}

TEST_F(DefaultBrowserPromptTest, RepromptDuration) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "2"}});

  // After the prompt is declined once, show the prompt again if the time since
  // the last time the prompt was declined is strictly longer than the base
  // reprompt duration.
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1),
      /*declined_count=*/1,
      /*expected=*/false);
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/1,
      /*expected=*/true);

  // If the user has declined the prompt multiple times, the next reprompt
  // duration should be multiplied by the reprompt multiplier for each
  // additional time the prompt has been declined.
  // So the prompt should be shown if the last declined time is older than:
  // base reprompt duration *
  //     (reprompt duration multiplier ^ (declined count - 1))

  // For example, after the prompt has been declined a second time, only show
  // the prompt (1 day) * (2^1) = 2 days after it was last declined.
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(2),
      /*declined_count=*/2,
      /*expected=*/false);
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(2) + base::Microseconds(1),
      /*declined_count=*/2,
      /*expected=*/true);

  // After the prompt has been declined a third time, only show the prompt
  // (1 day) * (2^2) = 4 days after it was last declined.
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(4),
      /*declined_count=*/3,
      /*expected=*/false);
  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/base::Days(4) + base::Microseconds(1),
      /*declined_count=*/3,
      /*expected=*/true);
}

TEST_F(DefaultBrowserPromptTest, PromptHiddenWhenFeatureParamDisabled) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "1"},
       {features::kShowDefaultBrowserInfoBar.name, "false"}});

  TestShouldShowDefaultBrowserPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/false);
}
