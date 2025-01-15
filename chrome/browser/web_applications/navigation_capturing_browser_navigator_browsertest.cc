// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace web_app {

namespace {
constexpr char kLandingPage[] = "/web_apps/simple/index.html";
constexpr char kRedirectFromPage[] = "/web_apps/simple2/index.html";
class NavigationCapturingBrowserNavigatorBrowserTest
    : public WebAppBrowserTestBase {
 public:
  NavigationCapturingBrowserNavigatorBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            apps::test::LinkCapturingFeatureVersion::kV2DefaultOff),
        {});
#else
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
        {});
#endif
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetLandingPage() const {
    return embedded_test_server()->GetURL(kLandingPage);
  }

  webapps::AppId InstallTestWebApp(const GURL& start_url,
                                   mojom::UserDisplayMode user_display_mode) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode = user_display_mode;
    const webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
              base::ok());
#endif
    return app_id;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateBrowserUserForBrowserTabAppLaunch) {
  // Test that the browser provided in NavigateParams is used when using a
  // browser to open a browser tab app in a tab, instead of the most recently
  // active browser.
  InstallTestWebApp(GetLandingPage(), mojom::UserDisplayMode::kBrowser);

  // Create a new browser which will be considered the most recently active one.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile());
  chrome::NewTab(new_browser);

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // the browser().
  ui_test_utils::AllBrowserTabAddedWaiter new_tab_observer;
  base::HistogramTester histograms;
  NavigateParams params(browser(), GetLandingPage(), ui::PAGE_TRANSITION_LINK);
  params.source_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  content::WebContents* new_tab = new_tab_observer.Wait();
  content::WaitForLoadStop(new_tab);

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);
  ASSERT_TRUE(new_tab);

  // Make sure that web contents is a tab in `browser()` and not `new_browser`.
  EXPECT_NE(browser()->tab_strip_model()->GetIndexOfWebContents(new_tab),
            TabStripModel::kNoTab);
}

class NavigationCapturingWithRedirectionBrowserNavigatorTest
    : public NavigationCapturingBrowserNavigatorBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Set up redirection before calling the `SetUpOnMainThread` on the parent
    // class, as that will start the server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &NavigationCapturingWithRedirectionBrowserNavigatorTest::
            HandleRedirection,
        base::Unretained(this)));
    NavigationCapturingBrowserNavigatorBrowserTest::SetUpOnMainThread();
  }

  GURL GetRedirectFromPage() const {
    return embedded_test_server()->GetURL(kRedirectFromPage);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirection(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL() != GetRedirectFromPage()) {
      return nullptr;
    }
    GURL redirect_to = GetLandingPage();
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", redirect_to.spec());
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StrCat(
        {"<!doctype html><p>Redirecting to %s", redirect_to.spec().c_str()}));
    return response;
  }
};

IN_PROC_BROWSER_TEST_F(NavigationCapturingWithRedirectionBrowserNavigatorTest,
                       NavigateBrowserUsedForBrowserTabAppLaunch) {
  // Test that the browser provided in NavigateParams is respected after it is
  // initially captured into an app window, only to be determined to need a
  // browser tabbed app after redirection.
  InstallTestWebApp(GetLandingPage(), mojom::UserDisplayMode::kBrowser);
  InstallTestWebApp(GetRedirectFromPage(), mojom::UserDisplayMode::kStandalone);

  // Create a new browser which will be considered the most recently active one.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile());
  chrome::NewTab(new_browser);

  // Do a capturable navigation to kRedirectFromPage (which redirects to
  // kLandingPage), and ensure that it opens in the browser().
  ui_test_utils::AllBrowserTabAddedWaiter new_tab_observer;
  base::HistogramTester histograms;
  NavigateParams params(browser(), GetRedirectFromPage(),
                        ui::PAGE_TRANSITION_LINK);
  params.source_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  content::WebContents* new_tab = new_tab_observer.Wait();
  content::WaitForLoadStop(new_tab);

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);

  // Make sure that web contents is a tab in `browser()` and not `new_browser`.
  // (perhaps there is a better way than calling FindBrowserWithTab).
  EXPECT_NE(browser()->tab_strip_model()->GetIndexOfWebContents(new_tab),
            TabStripModel::kNoTab);
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingWithRedirectionBrowserNavigatorTest,
                       NavigateBrowserUsedForBrowserTabLaunch) {
  // Test that the browser provided in NavigateParams is respected after it is
  // initially captured into an app window, only to be determined to need a
  // browser tab after redirection.
  InstallTestWebApp(GetRedirectFromPage(), mojom::UserDisplayMode::kStandalone);

  // Create a new browser which will be considered the most recently active one.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile());
  chrome::NewTab(new_browser);

  // Do a capturable navigation to kRedirectFromPage (which redirects to
  // kLandingPage), and ensure that it opens in the browser().
  ui_test_utils::AllBrowserTabAddedWaiter new_tab_observer;
  base::HistogramTester histograms;
  NavigateParams params(browser(), GetRedirectFromPage(),
                        ui::PAGE_TRANSITION_LINK);
  params.source_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  content::WebContents* new_tab = new_tab_observer.Wait();
  content::WaitForLoadStop(new_tab);
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 0);

  // Make sure that web contents is a tab in `browser()` and not `new_browser`.
  // (perhaps there is a better way than calling FindBrowserWithTab).
  EXPECT_NE(browser()->tab_strip_model()->GetIndexOfWebContents(new_tab),
            TabStripModel::kNoTab);
}
}  // namespace
}  // namespace web_app
