// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"

class HatsBubbleTest : public DialogBrowserTest {
 public:
  HatsBubbleTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(browser()->is_type_tabbed());
    BrowserView::GetBrowserViewForBrowser(InProcessBrowserTest::browser())
        ->ShowHatsBubbleFromAppMenuButton();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsBubbleTest);
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(HatsBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
