// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"

namespace web_app {

namespace {

constexpr char kLandingPage[] = "/web_apps/simple/index.html";
constexpr char kFinalPage[] = "/web_apps/simple/index2.html";
constexpr char kRedirectFromPage[] = "/web_apps/simple2/index.html";
constexpr char kAppNoManifestUrl[] = "/web_apps/no_manifest.html";
constexpr char kNavigateExistingUrl[] =
    "/web_apps/simple_navigate_existing/index.html";
constexpr char kNavigateExistingSecondUrl[] =
    "/web_apps/simple_navigate_existing/index2.html";
constexpr char kFocusExistingUrl[] =
    "/web_apps/simple_focus_existing/index.html";
constexpr char kFocusExistingSecondUrl[] =
    "/web_apps/simple_focus_existing/index2.html";

// Actually start a navigation in an existing web contents for the
// `navigate-existing` use-case.
// This is copied from `LoadURLInContents` in browser_navigator.cc.
void LoadURLInContents(content::WebContents* target_contents,
                       const GURL& url,
                       const NavigateParams& params) {
  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.initiator_frame_token = params.initiator_frame_token;
  load_url_params.initiator_process_id = params.initiator_process_id;
  load_url_params.initiator_origin = params.initiator_origin;
  load_url_params.initiator_base_url = params.initiator_base_url;
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.referrer = params.referrer;
  load_url_params.frame_name = params.frame_name;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.redirect_chain = params.redirect_chain;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;
  load_url_params.is_renderer_initiated = params.is_renderer_initiated;
  load_url_params.started_from_context_menu = params.started_from_context_menu;
  load_url_params.has_user_gesture = params.user_gesture;
  load_url_params.blob_url_loader_factory = params.blob_url_loader_factory;
  load_url_params.input_start = params.input_start;
  load_url_params.was_activated = params.was_activated;
  load_url_params.href_translate = params.href_translate;
  load_url_params.reload_type = params.reload_type;
  load_url_params.impression = params.impression;
  load_url_params.suggested_system_entropy = params.suggested_system_entropy;

  target_contents->GetController().LoadURLWithParams(load_url_params);
}

class NavigationCapturingBrowserNavigatorBrowserTest
    : public WebAppBrowserTestBase {
 public:
  NavigationCapturingBrowserNavigatorBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS)
    enabled_features = apps::test::GetFeaturesToEnableLinkCapturingUX(
        apps::test::LinkCapturingFeatureVersion::kV2DefaultOff);
#else
    enabled_features = apps::test::GetFeaturesToEnableLinkCapturingUX(
        apps::test::LinkCapturingFeatureVersion::kV2DefaultOn);
#endif

    enabled_features.emplace_back(blink::features::kDesktopPWAsTabStrip,
                                  base::FieldTrialParams());
    enabled_features.emplace_back(
        blink::features::kDesktopPWAsTabStripCustomizations,
        base::FieldTrialParams());

    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/enabled_features,
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetLandingPage() const {
    return embedded_test_server()->GetURL(kLandingPage);
  }

  GURL GetFinalPage() const {
    return embedded_test_server()->GetURL(kFinalPage);
  }

  GURL GetAppNoManifestUrl() const {
    return embedded_test_server()->GetURL(kAppNoManifestUrl);
  }

  GURL GetNavigateExistingUrl() const {
    return embedded_test_server()->GetURL(kNavigateExistingUrl);
  }

  GURL GetNavigateExistingSecondUrl() const {
    return embedded_test_server()->GetURL(kNavigateExistingSecondUrl);
  }

  GURL GetFocusExistingUrl() const {
    return embedded_test_server()->GetURL(kFocusExistingUrl);
  }

  GURL GetFocusExistingSecondUrl() const {
    return embedded_test_server()->GetURL(kFocusExistingSecondUrl);
  }

  // InstallTestWebApp should not be called with a url that has a manifest link.
  // This may cause flaky tests as it will be susceptible to manifest update as
  // soon as url loads. Instead, consider using test::InstallWebApp().
  webapps::AppId InstallTestWebApp(
      const GURL& start_url,
      mojom::UserDisplayMode user_display_mode,
      blink::mojom::ManifestLaunchHandler_ClientMode client_mode =
          blink::mojom::ManifestLaunchHandler_ClientMode::kAuto) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode = user_display_mode;
    web_app_info->launch_handler = blink::Manifest::LaunchHandler(client_mode);
    const webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
              base::ok());
#endif
    return app_id;
  }

  std::pair<Browser*, Browser*> GetTwoDistinctBrowsersForSameApp(
      const webapps::AppId& app_id) {
    // First, launch an app browser.
    Browser* app_browser_to_use = LaunchWebAppBrowser(app_id);

    // Second, create another app browser (LaunchWebAppBrowser() will not work
    // since the launch handling mode makes it look for an existing instance).
    // Mimic navigation capturing via shift click into a new window.
    Browser* second_app_browser = nullptr;
    {
      NavigateParams params(app_browser_to_use, GetLandingPage(),
                            ui::PAGE_TRANSITION_LINK);
      params.disposition = WindowOpenDisposition::NEW_WINDOW;
      Navigate(&params);
      second_app_browser = params.browser;
    }
    EXPECT_NE(nullptr, second_app_browser);
    EXPECT_NE(second_app_browser, app_browser_to_use);
    test::CompletePageLoadForAllWebContents();
    return {app_browser_to_use, second_app_browser};
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features;
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

// Test that the browser provided in NavigateParams is used when finding an app
// window for navigation capturing to happen for navigate-existing, provided the
// `app_id` is the same.
IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateBrowserUsedForNavigateExistingAppWindow) {
  const webapps::AppId& app_id = InstallTestWebApp(
      GetLandingPage(), mojom::UserDisplayMode::kStandalone,
      blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting);

  Browser* app_browser_1 = nullptr;
  Browser* app_browser_2 = nullptr;
  std::tie(app_browser_1, app_browser_2) =
      GetTwoDistinctBrowsersForSameApp(app_id);
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser_1, app_id));
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser_2, app_id));

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // `app_browser_1`.
  base::HistogramTester histograms;
  ui_test_utils::UrlLoadObserver url_observer(GetFinalPage());
  {
    NavigateParams params(app_browser_1, GetFinalPage(),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.source_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    Navigate(&params);
    LoadURLInContents(params.navigated_or_inserted_contents, GetFinalPage(),
                      params);
  }

  test::CompletePageLoadForAllWebContents();
  url_observer.Wait();
  content::WebContents* contents_navigation_happened_in =
      url_observer.web_contents();

  EXPECT_NE(contents_navigation_happened_in,
            app_browser_2->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(contents_navigation_happened_in,
            app_browser_1->tab_strip_model()->GetActiveWebContents());

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);
}

// Test that the browser provided in NavigateParams is used when finding an app
// window for navigation capturing to happen for focus-existing, provided the
// `app_id` is the same.
IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateBrowserUsedForFocusExistingAppWindow) {
  const webapps::AppId& app_id = InstallTestWebApp(
      GetLandingPage(), mojom::UserDisplayMode::kStandalone,
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);

  // Launch 2 distinct app_browsers for the same app_id. Since `app_browser_2`
  // is created last, ensure that is activated.
  Browser* app_browser_1 = nullptr;
  Browser* app_browser_2 = nullptr;
  std::tie(app_browser_1, app_browser_2) =
      GetTwoDistinctBrowsersForSameApp(app_id);
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser_1, app_id));
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser_2, app_id));

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // `app_browser_1`. Since the web_app has a client_mode of `focus-existing`,
  // `app_browser_1` should be activated with no navigations happening.
  base::HistogramTester histograms;
  {
    NavigateParams params(app_browser_1, GetFinalPage(),
                          ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }

  test::CompletePageLoadForAllWebContents();
  apps::test::FlushLaunchQueuesForAllBrowserTabs();

  content::WebContents* contents_to_finish =
      app_browser_1->tab_strip_model()->GetActiveWebContents();

  // `kLandingPage` should be obtained first when the browser is launched, and
  // `kFinalPage` is added later when navigation capturing happens.
  EXPECT_THAT(apps::test::GetLaunchParamUrlsInContents(
                  contents_to_finish, "launchParamsTargetUrls"),
              testing::ElementsAre(GetLandingPage(), GetFinalPage()));

  // `app_browser_1` should still be at the landing page.
  EXPECT_EQ(GetLandingPage(), app_browser_1->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL());

  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       FocusExistingWithBrowserAvoidsOutOfScope) {
  const webapps::AppId& app_id = InstallTestWebApp(
      GetLandingPage(), mojom::UserDisplayMode::kStandalone,
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);

  GURL out_of_scope =
      embedded_test_server()->GetURL("/web_apps/simple2/index.html");

  // Open the browser and navigate to out-of-scope url.
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, out_of_scope));
  content::WaitForLoadStop(
      app_browser->tab_strip_model()->GetActiveWebContents());

  // Populate the browser for a focus-existing navigation, which should reject
  // it because the current web contents is not in-scope of the app. And thus
  // create a new window.
  base::HistogramTester histograms;
  ui_test_utils::BrowserChangeObserver added(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  {
    NavigateParams params(app_browser, GetLandingPage(),
                          ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
  Browser* new_app_browser = added.Wait();
  test::CompletePageLoadForAllWebContents();

  EXPECT_EQ(out_of_scope, app_browser->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
  EXPECT_EQ(GetLandingPage(), new_app_browser->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL());

  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateExistingIgnoresNonHtml) {
  const webapps::AppId& app_id = InstallTestWebApp(
      GetLandingPage(), mojom::UserDisplayMode::kStandalone,
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);

  GURL image_url =
      embedded_test_server()->GetURL("/web_apps/simple/basic-48.png");

  // Open the browser and navigate to an in-scope image (non-html item);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, image_url));
  content::WaitForLoadStop(
      app_browser->tab_strip_model()->GetActiveWebContents());

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // a new browser;
  base::HistogramTester histograms;
  ui_test_utils::BrowserChangeObserver added(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  {
    NavigateParams params(browser(), GetLandingPage(),
                          ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
  Browser* new_app_browser = added.Wait();
  test::CompletePageLoadForAllWebContents();

  EXPECT_EQ(image_url, app_browser->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
  EXPECT_EQ(GetLandingPage(), new_app_browser->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL());

  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateBrowserUsedForNavigateExistingToBrowserTab) {
  // Test that the browser provided in NavigateParams is used when using a
  // browser to open a browser tab, instead of the most recently active browser.
  const webapps::AppId& app_id = InstallTestWebApp(
      GetAppNoManifestUrl(), mojom::UserDisplayMode::kBrowser,
      blink::mojom::ManifestLaunchHandler_ClientMode::kNavigateExisting);

  // Navigate to GetAppUrl() in browser() so that a browser tab for the app_id
  // exists.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppNoManifestUrl()));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  const content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_index = browser()->tab_strip_model()->GetIndexOfWebContents(contents);
  int tab_count_for_browser = browser()->tab_strip_model()->count();

  // Create a new browser which will be considered the most recently active
  // one.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile());
  chrome::NewTab(new_browser);

  // Do a capturable navigation to the landing page, and ensure that it
  // opens in the browser().
  base::HistogramTester histograms;
  ui_test_utils::UrlLoadObserver url_observer(GetFinalPage());
  {
    NavigateParams params(browser(), GetFinalPage(), ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        new_browser->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
    LoadURLInContents(params.navigated_or_inserted_contents, GetFinalPage(),
                      params);
  }

  test::CompletePageLoadForAllWebContents();
  url_observer.Wait();
  content::WebContents* contents_navigation_happened_in =
      url_observer.web_contents();

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);

  EXPECT_NE(contents_navigation_happened_in,
            new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(contents_navigation_happened_in,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(tab_index, browser()->tab_strip_model()->GetIndexOfWebContents(
                           contents_navigation_happened_in));
  EXPECT_EQ(tab_count_for_browser, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(NavigationCapturingBrowserNavigatorBrowserTest,
                       NavigateBrowserUsedForFocusExistingToBrowserTab) {
  // Test that the browser provided in NavigateParams is used when using a
  // browser to open a browser tab, instead of the most recently active browser.
  const webapps::AppId& app_id = InstallTestWebApp(
      GetAppNoManifestUrl(), mojom::UserDisplayMode::kBrowser,
      blink::mojom::ManifestLaunchHandler_ClientMode::kFocusExisting);

  // Navigate to GetAppUrl() in browser() so that a browser tab for the
  // app_id exists.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppNoManifestUrl()));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  int tab_count_for_browser = browser()->tab_strip_model()->count();

  // Create a new browser which will be considered the most recently active
  // one.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile());
  chrome::NewTab(new_browser);

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // browser(). Since the web_app has a client_mode of `focus-existing`,
  // browser() should be activated with no navigations happening.
  base::HistogramTester histograms;
  {
    NavigateParams params(browser(), GetFinalPage(), ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        new_browser->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }

  test::CompletePageLoadForAllWebContents();

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);

  // browser() should still be at the GetAppUrl() page.
  EXPECT_EQ(GetAppNoManifestUrl(), browser()
                                       ->tab_strip_model()
                                       ->GetActiveWebContents()
                                       ->GetLastCommittedURL());
  EXPECT_EQ(tab_count_for_browser, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(
    NavigationCapturingBrowserNavigatorBrowserTest,
    NavigateBrowserUsedForNavigateExistingToAppBrowserTabStandalone) {
  // Test that the app browser provided in NavigateParams is used even if a
  // separate browser is populated with a matching tab.
  const webapps::AppId& app_id = InstallWebAppFromPageAndCloseAppBrowser(
      browser(), GetNavigateExistingUrl());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
            base::ok());
#endif

  // Launch app in an app browser that will be passed in as the browser for
  // NavigateParams.
  Browser* app_browser_to_use = LaunchWebAppBrowser(app_id);

  // Change the web app's user display mode to kBrowser
  base::test::TestFuture<void> future;
  provider().scheduler().SetUserDisplayMode(
      app_id, mojom::UserDisplayMode::kBrowser, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Navigate to GetNavigateExistingUrl in browser() so that a browser tab for
  // the app_id exists.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetNavigateExistingUrl()));
  content::WebContents* blank_new_tab = chrome::AddAndReturnTabAt(
      browser(), GURL(url::kAboutBlankURL), /*index=*/0, /*foreground=*/true);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Do a capturable navigation to the landing page, and ensure that it
  // opens in the app_browser_to_use instead of browser().
  base::HistogramTester histograms;
  ui_test_utils::UrlLoadObserver url_observer(GetNavigateExistingSecondUrl());
  {
    NavigateParams params(app_browser_to_use, GetNavigateExistingSecondUrl(),
                          ui::PAGE_TRANSITION_LINK);
    params.source_contents = blank_new_tab;
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
    LoadURLInContents(params.navigated_or_inserted_contents,
                      GetNavigateExistingSecondUrl(), params);
  }

  test::CompletePageLoadForAllWebContents();
  url_observer.Wait();
  apps::test::FlushLaunchQueuesForAllBrowserTabs();
  content::WebContents* contents_navigation_happened_in =
      url_observer.web_contents();
  EXPECT_NE(contents_navigation_happened_in, blank_new_tab);

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);

  EXPECT_EQ(contents_navigation_happened_in,
            app_browser_to_use->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(0, app_browser_to_use->tab_strip_model()->GetIndexOfWebContents(
                   contents_navigation_happened_in));
  EXPECT_EQ(TabStripModel::kNoTab,
            browser()->tab_strip_model()->GetIndexOfWebContents(
                contents_navigation_happened_in));
}

IN_PROC_BROWSER_TEST_F(
    NavigationCapturingBrowserNavigatorBrowserTest,
    NavigateBrowserUsedForFocusExistingToAppBrowserTabStandalone) {
  // Test that the app browser provided in NavigateParams is used even if a
  // separate browser is populated with a matching tab.
  const webapps::AppId& app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), GetFocusExistingUrl());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(apps::test::EnableLinkCapturingByUser(profile(), app_id),
            base::ok());
#endif

  // Launch app in an app browser that will be passed in as the browser for
  // NavigateParams.
  Browser* app_browser_to_use = LaunchWebAppBrowser(app_id);

  // Change the web app's user display mode to kBrowser
  base::test::TestFuture<void> future;
  provider().scheduler().SetUserDisplayMode(
      app_id, mojom::UserDisplayMode::kBrowser, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Navigate to GetFocusExistingUrl() in browser() so that a browser tab for
  // the app_id exists.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetFocusExistingUrl()));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Do a capturable navigation to the landing page, and ensure that it opens in
  // browser(). Since the web_app has a client_mode of `focus-existing`,
  // browser() should be activated with no navigations happening.
  base::HistogramTester histograms;
  {
    NavigateParams params(app_browser_to_use, GetFocusExistingSecondUrl(),
                          ui::PAGE_TRANSITION_LINK);
    params.source_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }

  test::CompletePageLoadForAllWebContents();
  apps::test::FlushLaunchQueuesForAllBrowserTabs();

  // Ensure that capturing happened.
  histograms.ExpectUniqueSample(
      "WebApp.LaunchSource", apps::LaunchSource::kFromNavigationCapturing, 1);

  EXPECT_EQ(GetFocusExistingUrl(), app_browser_to_use->tab_strip_model()
                                       ->GetActiveWebContents()
                                       ->GetLastCommittedURL());
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
  EXPECT_NE(browser()->tab_strip_model()->GetIndexOfWebContents(new_tab),
            TabStripModel::kNoTab);
}
}  // namespace
}  // namespace web_app
