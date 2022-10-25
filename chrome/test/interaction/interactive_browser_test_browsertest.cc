// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

class InteractiveBrowserTestBrowsertest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
  }

  void TearDownOnMainThread() override {
    browser_view_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::raw_ptr<BrowserView> browser_view_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, SelectTab) {
  // Add at least three tabs.
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  RunTestSequence(
      SelectTab(kTabStripElementId, 1), Check(base::BindLambdaForTesting([&]() {
        return browser_view_->tabstrip()->GetActiveIndex() == 1;
      })),
      SelectTab(kTabStripElementId, 2), Check(base::BindLambdaForTesting([&]() {
        return browser_view_->tabstrip()->GetActiveIndex() == 2;
      })));
}
