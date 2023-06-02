// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"

#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace settings {

class PerformanceHandlerTest : public BrowserWithTestWindowTest {
 public:
  PerformanceHandlerTest() = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = std::make_unique<PerformanceHandler>();
    handler_->set_web_ui(web_ui());

    incognito_profile_ = TestingProfile::Builder().BuildIncognito(profile());
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();

    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PerformanceHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  TestingProfile* incognito_profile() { return incognito_profile_; }

  Browser* AddBrowser(Profile* profile) {
    Browser::CreateParams params(profile, true);
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  content::WebContents* AddTabToBrowser(Browser* browser, GURL url) {
    AddTab(browser, url);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  void ExpectCurrentOpenSitesEquals(std::vector<std::string> expected_sites,
                                    base::Value actual) {
    ASSERT_TRUE(actual.is_list());
    base::Value::List* actual_sites = actual.GetIfList();
    EXPECT_EQ(expected_sites.size(), actual_sites->size());
    for (size_t i = 0; i < expected_sites.size(); i++) {
      const base::Value& actual_site_value = (*actual_sites)[i];
      ASSERT_TRUE(actual_site_value.is_string());
      const std::string actual_site = actual_site_value.GetString();
      EXPECT_EQ(expected_sites[i], actual_site);
    }
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PerformanceHandler> handler_;
  TestingProfile profile_;
  raw_ptr<TestingProfile, DanglingUntriaged> incognito_profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  std::vector<std::unique_ptr<Browser>> browsers_;
};

TEST_F(PerformanceHandlerTest, GetCurrentOpenSites) {
  Browser* first_browser = AddBrowser(profile());
  AddTabToBrowser(first_browser, GURL("https://www.foo.com/ignorethispart"));
  content::WebContents* bar_tab =
      AddTabToBrowser(first_browser, GURL("https://bar.com"));
  AddTabToBrowser(first_browser, GURL("chrome://version"));

  Browser* second_browser = AddBrowser(profile());
  AddTabToBrowser(second_browser,
                  GURL("https://www.foo.com/ignorethispartaswell"));
  AddTabToBrowser(second_browser, GURL("http://www.baz.com"));

  Browser* incognito_browser = AddBrowser(incognito_profile());
  AddTabToBrowser(incognito_browser,
                  GURL("https://www.toshowthiswouldbeaprivacyviolation.com"));

  ExpectCurrentOpenSitesEquals({"www.baz.com", "www.foo.com", "bar.com"},
                               handler()->GetCurrentOpenSites());

  bar_tab->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
  ExpectCurrentOpenSitesEquals({"bar.com", "www.baz.com", "www.foo.com"},
                               handler()->GetCurrentOpenSites());
}

}  // namespace settings
