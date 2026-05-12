// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
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
    const base::ListValue& actual_sites = actual.GetList();
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

IN_PROC_BROWSER_TEST_F(PerformanceHandlerTest, GetCpuPerformanceInfo) {
  base::ListValue args;
  args.Append("callback-id");

  handler()->RegisterMessages();
  handler()->AllowJavascriptForTesting();

  // Call getCpuPerformanceInfo().
  web_ui()->HandleReceivedMessage("getCpuPerformanceInfo", args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("callback-id", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetBool());  // success

  const base::DictValue& info = data.arg3()->GetDict();
  std::optional<int> hardware_tier = info.FindInt("hardwareTier");
  ASSERT_TRUE(hardware_tier.has_value());
  EXPECT_TRUE(info.contains("model"));
  EXPECT_TRUE(info.contains("cores"));

  // Set an override and check that the handler still returns the hardware tier
  // (not the override).
  int override_tier = (*hardware_tier == 1) ? 2 : 1;
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCpuPerformanceTierOverride, override_tier);

  // Check that the overridden setting reads correctly via the browser client.
  std::optional<int> effective_override =
      content::GetContentClientForTesting()
          ->browser()
          ->GetCpuPerformanceTierOverride(browser()->profile());
  ASSERT_TRUE(effective_override.has_value());
  EXPECT_EQ(override_tier, *effective_override);

  // Call getCpuPerformanceInfo() again and check that the hardware tier has not
  // changed.
  web_ui()->HandleReceivedMessage("getCpuPerformanceInfo", args);
  const content::TestWebUI::CallData& data2 = *web_ui()->call_data().back();
  const base::DictValue& info2 = data2.arg3()->GetDict();
  EXPECT_EQ(*hardware_tier, info2.FindInt("hardwareTier"));
}

}  // namespace settings
