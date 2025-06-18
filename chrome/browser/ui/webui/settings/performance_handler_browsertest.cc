// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace settings {

class PerformanceHandlerTest : public InProcessBrowserTest {
 public:
  PerformanceHandlerTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = std::make_unique<PerformanceHandler>();
    handler_->set_web_ui(web_ui());
  }

  void TearDownOnMainThread() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
    web_contents_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PerformanceHandler* handler() { return handler_.get(); }

  content::WebContents* AddTabToBrowser(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  void ExpectCurrentOpenSitesEquals(std::vector<std::string> expected_sites,
                                    base::Value actual) {
    ASSERT_TRUE(actual.is_list());
    const base::Value::List& actual_sites = actual.GetList();
    ASSERT_EQ(expected_sites.size(), actual_sites.size());
    for (size_t i = 0; i < expected_sites.size(); i++) {
      const base::Value& actual_site_value = actual_sites[i];
      ASSERT_TRUE(actual_site_value.is_string());
      const std::string& actual_site = actual_site_value.GetString();
      EXPECT_EQ(expected_sites[i], actual_site);
    }
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PerformanceHandler> handler_;
  std::unique_ptr<content::WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(PerformanceHandlerTest, GetCurrentOpenSites) {
  Browser* first_browser = browser();
  AddTabToBrowser(first_browser, GURL("https://www.foo.com/ignorethispart"));
  content::WebContents* bar_tab =
      AddTabToBrowser(first_browser, GURL("https://bar.com"));
  AddTabToBrowser(first_browser, GURL("chrome://version"));

  Browser* second_browser = CreateBrowser(browser()->profile());
  AddTabToBrowser(second_browser,
                  GURL("https://www.foo.com/ignorethispartaswell"));
  AddTabToBrowser(second_browser, GURL("http://www.baz.com"));

  Browser* incognito_browser = CreateIncognitoBrowser();
  AddTabToBrowser(incognito_browser,
                  GURL("https://www.toshowthiswouldbeaprivacyviolation.com"));

  ExpectCurrentOpenSitesEquals({"www.baz.com", "www.foo.com", "bar.com"},
                               handler()->GetCurrentOpenSites());

  // Activate the tab with "bar.com" to test that it is moved to the front of
  // the list.
  TabStripModel* first_browser_tab_strip = first_browser->tab_strip_model();
  int bar_tab_index = first_browser_tab_strip->GetIndexOfWebContents(bar_tab);
  ASSERT_NE(bar_tab_index, TabStripModel::kNoTab);
  first_browser_tab_strip->ActivateTabAt(bar_tab_index);
  ASSERT_EQ(bar_tab->GetVisibility(), content::Visibility::VISIBLE);

  ExpectCurrentOpenSitesEquals({"bar.com", "www.baz.com", "www.foo.com"},
                               handler()->GetCurrentOpenSites());
}

}  // namespace settings
