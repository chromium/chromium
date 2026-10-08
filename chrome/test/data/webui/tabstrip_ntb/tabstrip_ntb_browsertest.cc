// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_service_impl.h"
#include "components/browser_apis/tab_strip/testing/injector.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tabstrip_ntb {
namespace {

class TabStripNtbBrowserTest : public WebUIMochaBrowserTest {
 protected:
  TabStripNtbBrowserTest() {
    set_test_loader_host(chrome::kChromeUITabStripNtbHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    tab_strip_service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::make_unique<tabs_api::testing::Injector>(tab_strip_));
  }

  void TearDownOnMainThread() override {
    if (content::WebContents* web_contents =
            chrome_test_utils::GetActiveWebContents(this)) {
      TabStripNtbUI::SetTabStripServiceForWebContents(web_contents, nullptr);
    }
    tab_strip_service_.reset();
    WebUIMochaBrowserTest::TearDownOnMainThread();
  }

  tabs_api::testing::ToyTabStrip tab_strip_;
  std::unique_ptr<tabs_api::TabStripService> tab_strip_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chrome::android::kAndroidNewTabButtonTabstripWebUI};
};

IN_PROC_BROWSER_TEST_F(TabStripNtbBrowserTest, ServiceSetBeforeConstruction) {
  base::UserActionTester user_action_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  TabStripNtbUI::SetTabStripServiceForWebContents(web_contents,
                                                  tab_strip_service_.get());

  const GURL url(
      base::StrCat({content::kChromeUIScheme, "://",
                    chrome::kChromeUITabStripNtbHost,
                    "/test_loader.html?module=tabstrip_ntb/"
                    "new_tab_button_test.js"}));
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  ASSERT_TRUE(RunTestOnWebContents(web_contents,
                                   "tabstrip_ntb/new_tab_button_test.js",
                                   "mocha.run()",
                                   /*skip_test_loader=*/false));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_.GetTabs().size() == 1u; }));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileToolbarNewTab"));
}

IN_PROC_BROWSER_TEST_F(TabStripNtbBrowserTest,
                       CallsBeforeServiceIsSetAreDelivered) {
  base::UserActionTester user_action_tester;
  RunTest("tabstrip_ntb/new_tab_button_test.js", "mocha.run()");

  TabStripNtbUI::SetTabStripServiceForWebContents(
      chrome_test_utils::GetActiveWebContents(this), tab_strip_service_.get());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_.GetTabs().size() == 1u; }));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileToolbarNewTab"));
}

}  // namespace
}  // namespace tabstrip_ntb
