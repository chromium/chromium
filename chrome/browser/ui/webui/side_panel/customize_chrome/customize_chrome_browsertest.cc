// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class CustomizeChromeSidePanelBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kCustomizeChromeSidePanel, features::kUnifiedSidePanel},
        {});
    InProcessBrowserTest::SetUp();
  }
  // Activates the browser tab at `index`.
  void ActivateTabAt(Browser* browser, int index);

  // Appends a new tab with `url` to the end of the tabstrip.
  void AppendTab(Browser* browser, const GURL& url);

  // Returns the CustomizeChromeTabHelper associated with the tab
  CustomizeChromeTabHelper* GetTabHelper(Browser* browser);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void CustomizeChromeSidePanelBrowserTest::ActivateTabAt(Browser* browser,
                                                        int index) {
  browser->tab_strip_model()->ActivateTabAt(index);
}

void CustomizeChromeSidePanelBrowserTest::AppendTab(Browser* browser,
                                                    const GURL& url) {
  chrome::AddTabAt(browser, url, -1, true);
}

CustomizeChromeTabHelper* CustomizeChromeSidePanelBrowserTest::GetTabHelper(
    Browser* browser) {
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  return CustomizeChromeTabHelper::FromWebContents(web_contents);
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       RegisterCustomizeChromeSidePanel) {
  auto* customize_chrome_tab_helper = GetTabHelper(browser());

  // When navigating to the New Tab Page, the Customize Chrome entry should be
  // available
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(customize_chrome_tab_helper->IsCustomizeChromeEntryAvailable());

  // After calling show, the customize chrome entry should be shown in the side
  // panel
  customize_chrome_tab_helper->ShowCustomizeChromeSidePanel();
  EXPECT_TRUE(customize_chrome_tab_helper->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       DeregisterCustomizeChromeSidePanel) {
  // If the Customize Chrome side panel is open and you navigate away from the
  // NTP the side panel entry should not be in the tabs' registry and the side
  // panel should not show the customize chrome entry
  auto* customize_chrome_tab_helper = GetTabHelper(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  customize_chrome_tab_helper->ShowCustomizeChromeSidePanel();
  EXPECT_TRUE(customize_chrome_tab_helper->IsCustomizeChromeEntryShowing());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));
  EXPECT_FALSE(customize_chrome_tab_helper->IsCustomizeChromeEntryAvailable());
  EXPECT_FALSE(customize_chrome_tab_helper->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       ContextualCustomizeChromeSidePanel) {
  // The Customize Chrome side panel should be contextual, opening on one tab
  // should not open it on other tabs.
  AppendTab(browser(), GURL(chrome::kChromeUINewTabURL));
  AppendTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ActivateTabAt(browser(), 1);
  // Navigate to URL to allow WebUI to load, if not then callback that is set
  // in the New Tab Page constructor and run when ShowCustomizeChromeSidePanel()
  // is called will not be set.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  auto* customize_chrome_tab_helper1 = GetTabHelper(browser());
  EXPECT_FALSE(customize_chrome_tab_helper1->IsCustomizeChromeEntryShowing());
  customize_chrome_tab_helper1->ShowCustomizeChromeSidePanel();
  ActivateTabAt(browser(), 2);
  auto* customize_chrome_tab_helper2 = GetTabHelper(browser());
  EXPECT_FALSE(customize_chrome_tab_helper2->IsCustomizeChromeEntryShowing());
}
