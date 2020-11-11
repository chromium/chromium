// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class TabSearchUIBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kTabSearch);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);

    webui_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));

    webui_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL(chrome::kChromeUITabSearchURL)));

    // Finish loading after initializing.
    ASSERT_TRUE(content::WaitForLoadStop(webui_contents_.get()));
  }

  void TearDownOnMainThread() override { webui_contents_.reset(); }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

  content::WebContents* GetActiveTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<content::WebContents> webui_contents_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(romanarora): Investigate a way to call WebUI custom methods and refactor
// JS code below.

IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, InitialTabItemsListed) {
  const std::string tab_items_js =
      "const tabItems = document.querySelector('tab-search-app').shadowRoot"
      "    .querySelectorAll('tab-search-item');";
  int tab_item_count =
      content::EvalJs(webui_contents_.get(), tab_items_js + "tabItems.length",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      ISOLATED_WORLD_ID_CHROME_INTERNAL)
          .ExtractInt();
  ASSERT_EQ(4, tab_item_count);
}

IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, SwitchToTabAction) {
  int tab_count = browser()->tab_strip_model()->GetTabCount();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1));
  ASSERT_EQ(tab_id, extensions::ExtensionTabUtil::GetTabId(GetActiveTab()));

  tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  const std::string tab_item_js = base::StringPrintf(
      "document.querySelector('tab-search-app').shadowRoot"
      "    .getElementById('%s')",
      base::NumberToString(tab_id).c_str());
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(), tab_item_js + ".click()",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
  ASSERT_EQ(tab_id, extensions::ExtensionTabUtil::GetTabId(GetActiveTab()));
}

IN_PROC_BROWSER_TEST_F(TabSearchUIBrowserTest, CloseTabAction) {
  ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  const std::string tab_item_button_js = base::StringPrintf(
      "document.querySelector('tab-search-app').shadowRoot.getElementById('%s')"
      "    .shadowRoot.getElementById('closeButton')",
      base::NumberToString(tab_id).c_str());
  ASSERT_TRUE(content::ExecJs(webui_contents_.get(),
                              tab_item_button_js + ".click()",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              ISOLATED_WORLD_ID_CHROME_INTERNAL));
  int tab_count = browser()->tab_strip_model()->GetTabCount();
  ASSERT_EQ(3, tab_count);

  std::vector<int> open_tab_ids(tab_count);
  for (int tab_index = 0; tab_index < tab_count; tab_index++) {
    open_tab_ids.push_back(extensions::ExtensionTabUtil::GetTabId(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index)));
  }
  ASSERT_EQ(open_tab_ids.end(),
            std::find(open_tab_ids.begin(), open_tab_ids.end(), tab_id));
}
