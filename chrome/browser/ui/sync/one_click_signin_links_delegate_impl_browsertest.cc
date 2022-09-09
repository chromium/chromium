// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_links_delegate_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using OneClickSigninLinksDelegateBrowserTest = InProcessBrowserTest;

// Disabled due to flakiness on Linux, https://crbug.com/1287471
#if BUILDFLAG(IS_LINUX)
#define MAYBE_LearnMoreLink DISABLED_LearnMoreLink
#else
#define MAYBE_LearnMoreLink LearnMoreLink
#endif
IN_PROC_BROWSER_TEST_F(OneClickSigninLinksDelegateBrowserTest,
                       MAYBE_LearnMoreLink) {
  std::unique_ptr<OneClickSigninLinksDelegate> delegate_(
      new OneClickSigninLinksDelegateImpl(browser()));

  int starting_tab_count = browser()->tab_strip_model()->count();

  // A new tab should be opened.
  delegate_->OnLearnMoreLinkClicked(false);

  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}
