// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class ChromeLabsButtonTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);
    TestWithBrowserView::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeLabsButtonTest, ShowAndHideChromeLabsBubbleOnPress) {
  ChromeLabsButton* labs_button =
      browser_view()->toolbar()->chrome_labs_button();
  EXPECT_FALSE(ChromeLabsBubbleView::IsShowing());
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(labs_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(ChromeLabsBubbleView::IsShowing());
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting()->GetWidget());
  test_api.NotifyClick(e);
  destroyed_waiter.Wait();
  EXPECT_FALSE(ChromeLabsBubbleView::IsShowing());
}
