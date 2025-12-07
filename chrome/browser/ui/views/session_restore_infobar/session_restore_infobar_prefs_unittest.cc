// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_prefs.h"

#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_restore_infobar {

class SessionRestoreInfoBarPrefsTest : public testing::Test {
 protected:
  SessionRestoreInfoBarPrefsTest() = default;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SessionRestoreInfoBarPrefsTest, InfobarShownMoreThenMaxTimes) {
  EXPECT_FALSE(InfoBarShownMaxTimes(profile_.GetPrefs()));

  IncrementInfoBarShownCount(profile_.GetPrefs());
  EXPECT_FALSE(InfoBarShownMaxTimes(profile_.GetPrefs()));

  IncrementInfoBarShownCount(profile_.GetPrefs());
  EXPECT_FALSE(InfoBarShownMaxTimes(profile_.GetPrefs()));

  IncrementInfoBarShownCount(profile_.GetPrefs());
  EXPECT_TRUE(InfoBarShownMaxTimes(profile_.GetPrefs()));

  IncrementInfoBarShownCount(profile_.GetPrefs());
  EXPECT_TRUE(InfoBarShownMaxTimes(profile_.GetPrefs()));
}

TEST_F(SessionRestoreInfoBarPrefsTest, InfoBarShownMaxTimes) {
  EXPECT_EQ(0, profile_.GetPrefs()->GetInteger(
                   prefs::kSessionRestoreInfoBarTimesShown));
  IncrementInfoBarShownCount(profile_.GetPrefs());

  EXPECT_EQ(1, profile_.GetPrefs()->GetInteger(
                   prefs::kSessionRestoreInfoBarTimesShown));
  IncrementInfoBarShownCount(profile_.GetPrefs());

  EXPECT_EQ(2, profile_.GetPrefs()->GetInteger(
                   prefs::kSessionRestoreInfoBarTimesShown));
  EXPECT_FALSE(InfoBarShownMaxTimes(profile_.GetPrefs()));
  IncrementInfoBarShownCount(profile_.GetPrefs());

  EXPECT_EQ(3, profile_.GetPrefs()->GetInteger(
                   prefs::kSessionRestoreInfoBarTimesShown));
  EXPECT_TRUE(InfoBarShownMaxTimes(profile_.GetPrefs()));
}

}  // namespace session_restore_infobar
