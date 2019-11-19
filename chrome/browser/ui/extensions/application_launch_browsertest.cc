// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

class ApplicationLaunchBrowserTest : public InProcessBrowserTest {
 public:
  ApplicationLaunchBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFocusMode);
  }

  content::WebContents* GetWebContentsForTab(Browser* browser, int index) {
    return browser->tab_strip_model()->GetWebContentsAt(index);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest,
                       ReparentWebContentsForFocusModeSingleTab) {
  const GURL url("http://aaa.com/empty.html");
  ui_test_utils::NavigateToURL(browser(), url);

  Browser* app_browser = web_app::ReparentWebContentsForFocusMode(
      GetWebContentsForTab(browser(), 0));
  EXPECT_TRUE(app_browser->is_type_app());
  EXPECT_NE(app_browser, browser());

  content::WebContents* main_browser_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(main_browser_web_contents);
  EXPECT_NE(url, main_browser_web_contents->GetLastCommittedURL());

  GURL app_browser_url = app_browser->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL();
  EXPECT_EQ(url, app_browser_url);
  EXPECT_TRUE(app_browser->is_focus_mode());
}

IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest,
                       ReparentWebContentsForFocusModeMultipleTabs) {
  const GURL url("http://aaa.com/empty.html");
  chrome::AddTabAt(browser(), url, -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_FALSE(browser()->is_focus_mode());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  Browser* app_browser = web_app::ReparentWebContentsForFocusMode(
      GetWebContentsForTab(browser(), 1));
  EXPECT_TRUE(app_browser->is_type_app());
  EXPECT_NE(app_browser, browser());
  EXPECT_EQ(url, GetWebContentsForTab(app_browser, 0)->GetURL());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(app_browser->is_focus_mode());
}
