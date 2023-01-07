// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/user_education/tip_marquee_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class TipMarqueeViewBrowserTest : public InProcessBrowserTest {
 public:
  TabStripRegionView* tab_strip_region_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->tab_strip_region_view();
  }

  TipMarqueeView* tip_marquee_view() {
    return tab_strip_region_view()->tip_marquee_view();
  }
};

IN_PROC_BROWSER_TEST_F(TipMarqueeViewBrowserTest, MarqueeStartsInvisibile) {
  EXPECT_FALSE(tip_marquee_view()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TipMarqueeViewBrowserTest,
                       VisibilityChangesOnSetAndClearTip) {
  tip_marquee_view()->SetAndShowTip(u"Tip Text");
  EXPECT_TRUE(tip_marquee_view()->GetVisible());
  tip_marquee_view()->ClearAndHideTip();
  EXPECT_FALSE(tip_marquee_view()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TipMarqueeViewBrowserTest, TipStartsExpanded) {
  tip_marquee_view()->SetAndShowTip(u"Tip Text");
  RunScheduledLayouts();
  EXPECT_EQ(tip_marquee_view()->GetPreferredSize(), tip_marquee_view()->size());
}
