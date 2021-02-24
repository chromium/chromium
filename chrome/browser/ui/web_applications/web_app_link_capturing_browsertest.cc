// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace web_app {

class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() {
    os_hooks_supress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void InstallTestApp(const char* path, bool await_metric) {
    start_url_ = embedded_test_server()->GetURL(path);
    in_scope_1_ = start_url_.Resolve("page1.html");
    in_scope_2_ = start_url_.Resolve("page2.html");
    scope_ = start_url_.GetWithoutFilename();

    // Create new tab to navigate, install, automatically pop out and then
    // close. This sequence avoids altering the browser window state we started
    // with.
    AddTab(browser(), about_blank_);

    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric) {
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppManifestCaptureLinks);
    }
    app_id_ = web_app::InstallWebAppFromPage(browser(), start_url_);
    if (await_metric)
      metrics_waiter.Wait();

    Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
    EXPECT_NE(app_browser, browser());
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
    chrome::CloseWindow(app_browser);
  }

  WebAppProviderBase& provider() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    DCHECK(provider);
    return *provider;
  }

  void AddTab(Browser* browser, GURL url) {
    auto observer = std::make_unique<content::TestNavigationObserver>(url);
    observer->StartWatchingNewWebContents();
    chrome::AddTabAt(browser, url, /*index=*/-1, /*foreground=*/true);
    observer->Wait();
  }

  void Navigate(Browser* browser, const GURL& url) {
    ClickLinkAndWait(browser->tab_strip_model()->GetActiveWebContents(), url,
                     LinkTarget::SELF, "");
  }

  void ExpectTabs(Browser* browser, std::vector<GURL> urls) {
    TabStripModel& tab_strip = *browser->tab_strip_model();
    ASSERT_EQ(static_cast<size_t>(tab_strip.count()), urls.size());
    for (int i = 0; i < tab_strip.count(); ++i) {
      SCOPED_TRACE(base::StringPrintf("is app browser: %d, tab index: %d",
                                      bool(browser->app_controller()), i));
      EXPECT_EQ(
          browser->tab_strip_model()->GetWebContentsAt(i)->GetVisibleURL(),
          urls[i]);
    }
  }

  GURL NtpUrl() {
    return local_ntp_test_utils::GetFinalNtpUrl(browser()->profile());
  }

 protected:
  AppId app_id_;
  GURL start_url_;
  GURL in_scope_1_;
  GURL in_scope_2_;
  GURL scope_;

  const GURL about_blank_{"about:blank"};
  const GURL out_of_scope_{"https://other-domain.org/"};

  ScopedOsHooksSuppress os_hooks_supress_;
};

class WebAppTabStripLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest {
 public:
  WebAppTabStripLinkCapturingBrowserTest() {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripLinkCapturing},
                               {});
  }

  void InstallTestApp() {
    WebAppLinkCapturingBrowserTest::InstallTestApp("/web_apps/basic.html",
                                                   /*await_metric=*/false);
    provider().registry_controller().SetExperimentalTabbedWindowMode(
        app_id_, true, /*is_user_action=*/false);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  InstallTestApp();

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Navigate(browser(), in_scope_1_);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // Another in scope navigation should open a new tab in the same app window.
  Navigate(browser(), in_scope_2_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});

  // Whole origin should count as in scope.
  Navigate(browser(), scope_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});

  // Middle clicking links should not be captured.
  ClickLinkWithModifiersAndWaitForURL(
      browser()->tab_strip_model()->GetActiveWebContents(), scope_, scope_,
      LinkTarget::SELF, "", blink::WebInputEvent::Modifiers::kNoModifiers,
      blink::WebMouseEvent::Button::kMiddle);
  ExpectTabs(browser(), {out_of_scope_, scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});

  // Out of scope should behave as usual.
  Navigate(browser(), out_of_scope_);
  ExpectTabs(browser(), {out_of_scope_, scope_});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_, scope_});
}

// First about:blank captures in scope navigations.
IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       AboutBlankNavigationReparented) {
  InstallTestApp();

  ExpectTabs(browser(), {about_blank_});
  content::WebContents* reparent_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigations from a fresh about:blank page should reparent.
  // When no app window is open one should be created.
  Navigate(browser(), in_scope_1_);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());

  // Navigations from a fresh about:blank page via JavaScript should also
  // reparent. When there is already an app window open we should reparent into
  // it.
  AddTab(browser(), about_blank_);
  ExpectTabs(browser(), {NtpUrl(), about_blank_});
  reparent_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  {
    auto observer =
        std::make_unique<content::TestNavigationObserver>(in_scope_2_);
    observer->WatchExistingWebContents();
    ASSERT_TRUE(content::ExecuteScript(
        reparent_web_contents,
        base::StringPrintf("location = '%s';", in_scope_2_.spec().c_str())));
    observer->Wait();
  }
  ExpectTabs(browser(), {NtpUrl()});
  ExpectTabs(app_browser, {in_scope_1_, in_scope_2_});
  EXPECT_EQ(reparent_web_contents,
            app_browser->tab_strip_model()->GetActiveWebContents());
}

class WebAppDeclarativeLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest {
 public:
  WebAppDeclarativeLinkCapturingBrowserTest() {
    features_.InitAndEnableFeature(blink::features::kWebAppEnableLinkCapturing);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksUnset) {
  InstallTestApp("/web_apps/basic.html", /*await_metric=*/false);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 0);

  // No link capturing should happen.
  Navigate(browser(), start_url_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {start_url_});
}

IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksNone) {
  InstallTestApp("/web_apps/capture_links_none.html", /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  // No link capturing should happen.
  Navigate(browser(), start_url_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {start_url_});
}

// Flaky test: https://crbug.com/1167176
IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingBrowserTest,
                       DISABLED_CaptureLinksNewClient) {
  InstallTestApp("/web_apps/capture_links_new_client.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  Navigate(browser(), out_of_scope_);

  // In scope navigation should open an app window.
  Navigate(browser(), in_scope_1_);
  Browser* app_browser_1 = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser_1, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_1_});

  // In scope navigation should open a new app window.
  Navigate(browser(), in_scope_2_);
  Browser* app_browser_2 = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser_2, app_id_));
  EXPECT_NE(app_browser_1, app_browser_2);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_1_});
  ExpectTabs(app_browser_2, {in_scope_2_});

  // In scope navigation from app window should not capture.
  Navigate(app_browser_1, in_scope_2_);
  EXPECT_EQ(app_browser_2, BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser_1, {in_scope_2_});
  ExpectTabs(app_browser_2, {in_scope_2_});
}

IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingBrowserTest,
                       InAppScopeNavigationIgnored) {
  InstallTestApp("/web_apps/capture_links_new_client.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  // Start browser in app scope.
  AddTab(browser(), in_scope_1_);

  // Navigations that happen inside the app scope should not capture even if
  // done outside of an app window.
  Navigate(browser(), in_scope_2_);
  EXPECT_EQ(browser(), BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {about_blank_, in_scope_2_});
}

IN_PROC_BROWSER_TEST_F(WebAppDeclarativeLinkCapturingBrowserTest,
                       CaptureLinksExistingClientNavigate) {
  InstallTestApp("/web_apps/capture_links_existing_client_navigate.html",
                 /*await_metric=*/true);

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kWebAppManifestCaptureLinks, 1);

  Navigate(browser(), out_of_scope_);

  // In scope navigation should open an app window (because there are none
  // already open).
  Navigate(browser(), in_scope_1_);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // In scope navigation should navigate the existing app window.
  Navigate(browser(), in_scope_2_);
  EXPECT_EQ(app_browser, BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_2_});

  // In scope navigation should navigate the existing app window.
  Navigate(browser(), in_scope_1_);
  EXPECT_EQ(app_browser, BrowserList::GetInstance()->GetLastActive());
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});
}

}  // namespace web_app
