// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
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

  PrefService *profile_prefs() { return testing_profile->GetPrefs(); }

  Profile *profile() { return testing_profile; }

  PrefService *local_state() { return g_browser_process->local_state(); }

private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
