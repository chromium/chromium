// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_prefs.h"

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf::infobar {

class PdfInfoBarPrefsTest : public testing::Test {
 protected:
  PdfInfoBarPrefsTest() : profile_(std::make_unique<TestingProfile>()) {}

  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }
  TestingProfile* profile() { return profile_.get(); }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const std::unique_ptr<TestingProfile> profile_;
};

TEST_F(PdfInfoBarPrefsTest, SetInfoBarShownRecently) {
  SetInfoBarShownRecently();
  EXPECT_EQ(1, local_state()->GetInteger(prefs::kPdfInfoBarTimesShown));
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kPdfInfoBarLastShown));

  FastForwardBy(base::Hours(1));
  SetInfoBarShownRecently();
  EXPECT_EQ(2, local_state()->GetInteger(prefs::kPdfInfoBarTimesShown));
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kPdfInfoBarLastShown));
}

TEST_F(PdfInfoBarPrefsTest, InfoBarShownRecentlyOrMaxTimes) {
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
  local_state()->SetInteger(prefs::kPdfInfoBarTimesShown, 1);
  local_state()->SetTime(prefs::kPdfInfoBarLastShown, base::Time::Now());
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
}

TEST_F(PdfInfoBarPrefsTest, InfoBarShownRecentlyOrMaxTimesExponential) {
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
  SetInfoBarShownRecently();
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());

  // The infobar was shown once, so it shouldn't be shown again for
  // `kPdfInfoBarShowIntervalDays` days.
  FastForwardBy(base::Days(kPdfInfoBarShowIntervalDays - 1));
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
  FastForwardBy(base::Days(1));
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
  SetInfoBarShownRecently();

  // The infobar was shown twice, so it shouldn't be shown again for
  // `kPdfInfoBarShowIntervalDays * 2` days.
  FastForwardBy(base::Days((kPdfInfoBarShowIntervalDays * 2) - 1));
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
  FastForwardBy(base::Days(1));
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
  SetInfoBarShownRecently();

  // The infobar was shown three times, so it shouldn't be shown again for
  // `kPdfInfoBarShowIntervalDays * 4` days.
  FastForwardBy(base::Days((kPdfInfoBarShowIntervalDays * 4) - 1));
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
  FastForwardBy(base::Days(1));
  EXPECT_FALSE(InfoBarShownRecentlyOrMaxTimes());
}

TEST_F(PdfInfoBarPrefsTest, InfoBarShownRecentlyOrMaxTimesMaxReached) {
  local_state()->SetInteger(prefs::kPdfInfoBarTimesShown,
                            kPdfInfoBarMaxTimesToShow);
  local_state()->SetTime(prefs::kPdfInfoBarLastShown,
                         base::Time::Now() - base::Days(3650));
  // The infobar has been shown the max number of times, so it shouldn't be
  // shown again regardless of how much time has passed.
  EXPECT_TRUE(InfoBarShownRecentlyOrMaxTimes());
}

TEST_F(PdfInfoBarPrefsTest, IsPdfViewerDisabled) {
  EXPECT_FALSE(IsPdfViewerDisabled(profile()));
  profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    false);
  EXPECT_FALSE(IsPdfViewerDisabled(profile()));
  profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    true);
  EXPECT_TRUE(IsPdfViewerDisabled(profile()));
}

TEST_F(PdfInfoBarPrefsTest, IsDefaultBrowserPolicyControlled) {
  EXPECT_FALSE(IsDefaultBrowserPolicyControlled());
  local_state()->SetManagedPref(prefs::kDefaultBrowserSettingEnabled,
                                base::Value(false));
  EXPECT_FALSE(IsDefaultBrowserPolicyControlled());
  local_state()->SetManagedPref(prefs::kDefaultBrowserSettingEnabled,
                                base::Value(true));
  EXPECT_TRUE(IsDefaultBrowserPolicyControlled());
}

}  // namespace pdf::infobar
