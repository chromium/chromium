// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

const char kAppHost[] = "app.com";
const char kApp2Host[] = "app2.com";

}  // namespace

class IsolatedAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  IsolatedAppBrowserTest()
      : scoped_feature_list_(blink::features::kWebAppEnableIsolatedStorage) {}

  IsolatedAppBrowserTest(const IsolatedAppBrowserTest&) = delete;
  IsolatedAppBrowserTest& operator=(const IsolatedAppBrowserTest&) = delete;
  ~IsolatedAppBrowserTest() override = default;

 protected:
  AppId InstallIsolatedApp(const std::string& host) {
    GURL app_url = https_server()->GetURL(host,
                                          "/banners/manifest_test_page.html"
                                          "?manifest=manifest_isolated.json");
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    return test::InstallPwaForCurrentUrl(browser());
  }

  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::RenderFrameHost* GetMainFrame(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserTest, AppsPartitioned) {
  InstallIsolatedApp(kAppHost);
  InstallIsolatedApp(kApp2Host);

  auto* non_app_frame = ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/banners/isolated/simple.html"));
  EXPECT_TRUE(non_app_frame);
  EXPECT_EQ(default_storage_partition(), non_app_frame->GetStoragePartition());

  auto* app_window = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL(kAppHost, "/banners/isolated/simple.html"));
  auto* app_frame = GetMainFrame(app_window);
  EXPECT_NE(default_storage_partition(), app_frame->GetStoragePartition());

  auto* app2_window = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL(kApp2Host, "/banners/isolated/simple.html"));
  auto* app2_frame = GetMainFrame(app2_window);
  EXPECT_NE(default_storage_partition(), app2_frame->GetStoragePartition());

  EXPECT_NE(app_frame->GetStoragePartition(),
            app2_frame->GetStoragePartition());
}

}  // namespace web_app
