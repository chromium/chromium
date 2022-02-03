// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"

namespace web_app {

enum class FlagParam {
  kWithFlag = 0,
  kWithoutFlag = 1,
  kMaxValue = kWithoutFlag
};

class WebAppOfflinePageTest : public InProcessBrowserTest,
                              public ::testing::WithParamInterface<FlagParam> {
 public:
  WebAppOfflinePageTest() {
    if (GetParam() == FlagParam::kWithFlag) {
      feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsDefaultOfflinePage);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsDefaultOfflinePage);
    }
  }

  // Start a web app without a service worker and disconnect.
  void StartWebAppAndDisconnect(content::WebContents* web_contents,
                                std::string html) {
    GURL target_url(embedded_test_server()->GetURL(html));
    web_app::NavigateToURLAndWait(browser(), target_url);
    web_app::test::InstallPwaForCurrentUrl(browser());

    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  // Start a PWA with a service worker and disconnect.
  void StartPwaAndDisconnect(content::WebContents* web_contents,
                             std::string html) {
    GURL target_url(embedded_test_server()->GetURL(html));
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(
        browser()->profile(), target_url);
    web_app::NavigateToURLAndWait(browser(), target_url);
    registration_waiter.AwaitRegistration();
    web_app::test::InstallPwaForCurrentUrl(browser());

    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When a web app with a manifest and no service worker is offline it should
// display the default offline page rather than the dino.
// When the exact same conditions are applied with the feature flag disabled
// expect that the default offline page is not shown.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflinePageIsDisplayed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  if (GetParam() == FlagParam::kWithFlag) {
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());
  } else {
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());
  }
}

// When a web app with a manifest and service worker that doesn't handle being
// offline it should display the default offline page rather than the dino.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest,
                       WebAppOfflineWithEmptyServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartPwaAndDisconnect(web_contents, "/banners/background-color.html");

  if (GetParam() == FlagParam::kWithFlag) {
    // Expect that the default offline page is showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') !== null")
            .ExtractBool());
  } else {
    // Expect that the default offline page is not showing.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "document.getElementById('default-web-app-msg') === null")
            .ExtractBool());
  }
}

// When a web app with a manifest and service worker that handles being offline
// it should not display the default offline page.
IN_PROC_BROWSER_TEST_P(WebAppOfflinePageTest, WebAppOfflineWithServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartPwaAndDisconnect(web_contents, "/banners/theme-color.html");

  // Expect that the default offline page is not showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') === null")
                  .ExtractBool());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppOfflinePageTest,
                         ::testing::Values(FlagParam::kWithFlag,
                                           FlagParam::kWithoutFlag));

}  // namespace web_app
