// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_links_delegate_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using OneClickSigninLinksDelegateBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(OneClickSigninLinksDelegateBrowserTest, LearnMoreLink) {
  std::unique_ptr<OneClickSigninLinksDelegate> delegate_(
      new OneClickSigninLinksDelegateImpl(browser()));

  int starting_tab_count = browser()->tab_strip_model()->count();

  // A new tab should be opened.
  delegate_->OnLearnMoreLinkClicked(false);

  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}
