// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_prefs.h"

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

class PinInfoBarPrefsTest : public testing::Test {
 protected:
  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PinInfoBarPrefsTest, SetInfoBarShownRecently) {
  SetInfoBarShownRecently();
  EXPECT_EQ(1, local_state()->GetInteger(prefs::kPinInfoBarTimesShown));
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kPinInfoBarLastShown));

  FastForwardBy(base::Hours(1));
  SetInfoBarShownRecently();
  EXPECT_EQ(2, local_state()->GetInteger(prefs::kPinInfoBarTimesShown));
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kPinInfoBarLastShown));
}

TEST_F(PinInfoBarPrefsTest, InfoBarShownRecentlyOrMaxTimes) {
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());

  local_state()->SetInteger(prefs::kPinInfoBarTimesShown, 1);
  local_state()->SetTime(prefs::kPinInfoBarLastShown, base::Time::Now());
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());

  FastForwardBy(base::Days(kPinInfoBarRepromptDays));
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
}

TEST_F(PinInfoBarPrefsTest, InfoBarShownRecentlyOrMaxTimesMaxReached) {
  local_state()->SetInteger(prefs::kPinInfoBarTimesShown,
                            kPinInfoBarMaxPromptCount);
  local_state()->SetTime(prefs::kPinInfoBarLastShown,
                         base::Time::Now() - base::Days(3650));
  // The infobar has been shown the max number of times, so it shouldn't be
  // shown again regardless of how much time has passed.
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
}

}  // namespace default_browser
