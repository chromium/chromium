// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/install_prompt_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webapps/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

class InstallPromptPrefsTest : public ::testing::Test {
 public:
  void SetUp() override {
    // user_prefs::UserPrefs::Set(browser_context(), &prefs_);
    InstallPromptPrefs::RegisterProfilePrefs(prefs_.registry());
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(InstallPromptPrefsTest, PromptDismissedRecently) {
  base::Time time = base::Time::Now();
  // Dismiss the prompt once.
  InstallPromptPrefs::RecordInstallPromptDismissed(prefs(), time);
  EXPECT_FALSE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time));

  // Dismiss 2 more times (3 times total).
  InstallPromptPrefs::RecordInstallPromptDismissed(prefs(), time);
  InstallPromptPrefs::RecordInstallPromptDismissed(prefs(), time);
  EXPECT_TRUE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time));

  // Dismisses do not considered "recent" after 7 days.
  EXPECT_FALSE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time + base::Days(7)));
}

TEST_F(InstallPromptPrefsTest, PromptIgnoredRecently) {
  base::Time time = base::Time::Now();
  // Ignore the prompt once.
  InstallPromptPrefs::RecordInstallPromptIgnored(prefs(), time);
  EXPECT_FALSE(
      InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(prefs(), time));

  // Ignore 2 more times (3 times total).
  InstallPromptPrefs::RecordInstallPromptIgnored(prefs(), time);
  InstallPromptPrefs::RecordInstallPromptIgnored(prefs(), time);
  EXPECT_TRUE(
      InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(prefs(), time));

  // Ignore do not considered "recent" after 3 days.
  EXPECT_FALSE(InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(
      prefs(), time + base::Days(3)));
}

TEST_F(InstallPromptPrefsTest, RecentCountResetByClicks) {
  base::Time time = base::Time::Now();
  // Record 3 dismiss and 3 ignore.
  for (int i = 0; i < 3; i++) {
    InstallPromptPrefs::RecordInstallPromptDismissed(prefs(), time);
    InstallPromptPrefs::RecordInstallPromptIgnored(prefs(), time);
  }
  EXPECT_TRUE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time));
  EXPECT_TRUE(
      InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(prefs(), time));

  // Record one click, both dimiss and ignore are not continuous.
  InstallPromptPrefs::RecordInstallPromptClicked(prefs());
  EXPECT_FALSE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time));
  EXPECT_FALSE(
      InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(prefs(), time));

  // 2 more dimisses & ignores, still ok.
  for (int i = 0; i < 2; i++) {
    InstallPromptPrefs::RecordInstallPromptDismissed(prefs(), time);
    InstallPromptPrefs::RecordInstallPromptIgnored(prefs(), time);
  }
  EXPECT_FALSE(InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
      prefs(), time));
  EXPECT_FALSE(
      InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(prefs(), time));
}

}  // namespace webapps
