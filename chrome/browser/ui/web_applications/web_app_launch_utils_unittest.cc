// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"

using content::NavigationSimulator;

using WebAppLaunchUtilsTest = WebAppTest;

TEST_F(WebAppLaunchUtilsTest, PruneHistory) {
  const GURL scope("https://example.com/a/");

  const GURL first("https://example.com/a/first");
  const GURL second("https://example.com/a/second");
  const GURL third("https://example.com/b/third");  // Out of scope.
  const GURL fourth("https://example.com/a/fourth");
  const GURL fifth("https://example.com/a/fifth");

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), first);
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), second);
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), third);
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), fourth);
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), fifth);

  web_app::PrunePreScopeNavigationHistory(scope, web_contents());
  EXPECT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  EXPECT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            fourth);
  EXPECT_EQ(web_contents()->GetController().GetEntryAtIndex(1)->GetURL(),
            fifth);
}
