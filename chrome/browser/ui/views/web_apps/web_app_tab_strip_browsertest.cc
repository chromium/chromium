// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/stack_allocated.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

constexpr SkColor kAppBackgroundColor = SK_ColorBLUE;

// The page shouldn't have a manifest so no updating will occur to override
// our settings.
constexpr char kAppPath[] = "/web_apps/no_manifest.html";

}  // namespace
namespace web_app {

class WebAppTabStripBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppTabStripBrowserTest() = default;
  ~WebAppTabStripBrowserTest() override = default;

  void SetUp() override {
    features_.InitWithFeatures(
        {blink::features::kDesktopPWAsTabStrip,
         features::kDesktopPWAsTabStripSettings,
         blink::features::kDesktopPWAsTabStripCustomizations},
        {});
    ASSERT_TRUE(embedded_test_server()->Start());

    WebAppBrowserTestBase::SetUp();
  }

  struct App {
    STACK_ALLOCATED();

   public:
    webapps::AppId id;
    Browser* browser;
    BrowserView* browser_view;
    content::WebContents* web_contents;
  };

  webapps::AppId Install() {
    Profile* profile = browser()->profile();
    GURL start_url = embedded_test_server()->GetURL(kAppPath);

    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test app";
    web_app_info->background_color = kAppBackgroundColor;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->display_override = {blink::mojom::DisplayMode::kTabbed};
    return test::InstallWebApp(profile, std::move(web_app_info));
  }

  App InstallAndLaunch() {
    Profile* profile = browser()->profile();
    webapps::AppId app_id = Install();

    Browser* app_browser = ::web_app::LaunchWebAppBrowser(profile, app_id);
    return App{app_id, app_browser,
               BrowserView::GetBrowserViewForBrowser(app_browser),
               app_browser->tab_strip_model()->GetActiveWebContents()};
  }

  webapps::AppId InstallTestWebApp(GURL start_url, bool await_metric = true) {
    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric) {
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppTabbed);
    }
    webapps::AppId app_id = InstallWebAppFromPage(browser(), start_url);
    if (await_metric) {
      metrics_waiter.Wait();
    }
    return app_id;
  }

  void OpenUrlAndWait(Browser* app_browser, GURL url) {
    TabStripModel* tab_strip = app_browser->tab_strip_model();

    content::WaitForLoadStop(tab_strip->GetActiveWebContents());

    NavigateParams params(app_browser, url,
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    ui_test_utils::NavigateToURL(&params);
  }

  SkColor GetTabColor(BrowserView* browser_view) {
    return TabStyle::Get()->GetTabBackgroundColor(
        TabStyle::TabSelectionState::kActive, /*hovered=*/false,
        /*frame_active=*/true, *browser_view->GetColorProvider());
  }

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar_unsafe();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       CustomTabBarUpdateOnTabSwitch) {
  App app = InstallAndLaunch();

  CustomTabBarView* custom_tab_bar =
      app.browser_view->toolbar()->custom_tab_bar();
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Add second tab.
  chrome::NewTab(app.browser);
  ASSERT_EQ(app.browser->tab_strip_model()->count(), 2);

  // Navigate tab out of scope, custom tab bar should appear.
  GURL in_scope_url = embedded_test_server()->GetURL(kAppPath);
  GURL out_of_scope_url =
      embedded_test_server()->GetURL("/banners/theme-color.html");
  ASSERT_TRUE(content::NavigateToURL(
      app.browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_url));
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());

  // Custom tab bar should go away for in scope tab.
  chrome::SelectNextTab(app.browser);
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      in_scope_url);
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Custom tab bar should return for out of scope tab.
  chrome::SelectNextTab(app.browser);
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, PopOutTabOnInstall) {
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");

  NavigateViaLinkClickToURLAndWait(browser(), start_url);

  // Install the site with the user display mode set to kTabbed.
  Browser* app_browser;
  webapps::AppId app_id;
  {
    ui_test_utils::BrowserChangeObserver app_browser_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    base::RunLoop run_loop;
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider);
    test::WaitUntilReady(provider);
    provider->scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::MENU_BROWSER_TAB,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindLambdaForTesting(
            [](content::WebContents*,
               std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
               WebAppInstallationAcceptanceCallback acceptance_callback) {
              web_app_info->user_display_mode = mojom::UserDisplayMode::kTabbed;
              std::move(acceptance_callback)
                  .Run(/*user_accepted=*/true, std::move(web_app_info));
            }),
        base::BindLambdaForTesting(
            [&run_loop, &app_id](const webapps::AppId& installed_app_id,
                                 webapps::InstallResultCode code) {
              DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              app_id = installed_app_id;
              run_loop.Quit();
            }),
        FallbackBehavior::kAllowFallbackDataAlways);
    run_loop.Run();
    app_browser = app_browser_observer.Wait();
    ASSERT_TRUE(app_browser);
    EXPECT_NE(app_browser, browser());
    ui_test_utils::WaitUntilBrowserBecomeActive(app_browser);
  }

  // After installing a tabbed display mode app the install page should pop out
  // to a standalone app window.
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL("chrome://newtab/"));
}

// TODO(crbug.com/40598974) Enabled tab strip for web apps on non-Chrome OS.
#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       ActiveTabColorIsBackgroundColor) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDefaultTheme();

  webapps::AppId app_id = Install();

  // Trigger the launch but do not wait for the web contents to load.
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              apps::LaunchSource::kFromTest));
  ASSERT_TRUE(web_contents);
  Browser* app_browser = chrome::FindBrowserWithTab(web_contents);
  App app{app_id, app_browser,
          BrowserView::GetBrowserViewForBrowser(app_browser), web_contents};

  EXPECT_EQ(registrar().GetAppBackgroundColor(app.id), kAppBackgroundColor);

  // Expect manifest background color prior to page loading.
  {
    ASSERT_FALSE(
        app.web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              kAppBackgroundColor);
    EXPECT_EQ(GetTabColor(app.browser_view), kAppBackgroundColor);
  }

  // Expect initial page background color to be white.
  {
    content::BackgroundColorChangeWaiter(app.web_contents).Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorWHITE);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorWHITE);
  }

  // Ensure HTML document has loaded before we execute JS in it.
  content::AwaitDocumentOnLoadCompleted(app.web_contents);

  // Set document color to black and read tab background color.
  {
    content::BackgroundColorChangeWaiter waiter(app.web_contents);
    EXPECT_TRUE(content::ExecJs(
        app.web_contents, "document.body.style.backgroundColor = 'black';"));
    waiter.Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorBLACK);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorBLACK);
  }

  // Update document color to cyan and check that the tab color matches.
  {
    content::BackgroundColorChangeWaiter waiter(app.web_contents);
    EXPECT_TRUE(content::ExecJs(
        app.web_contents, "document.body.style.backgroundColor = 'cyan';"));
    waiter.Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorCYAN);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorCYAN);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, AutoNewTabUrl) {
  GURL start_url = embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_tabbed_display_override.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  chrome::NewTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      registrar().GetAppStartUrl(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, NewTabUrl) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  chrome::NewTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, NonTabbedWebApp) {
  Profile* profile = browser()->profile();

  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      embedded_test_server()->GetURL(kAppPath));
  web_app_info->title = u"Test app";
  webapps::AppId app_id = test::InstallWebApp(profile, std::move(web_app_info));

  Browser* app_browser = web_app::LaunchWebAppBrowser(profile, app_id);

  EXPECT_TRUE(app_browser->app_controller()->ShouldHideNewTabButton());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, InstallingPinsHomeTab) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/tab_strip_customizations.html#blah");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  // The URL of the pinned home tab should include the fragment.
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // App should have a new tab button.
  EXPECT_FALSE(app_browser->app_controller()->ShouldHideNewTabButton());
}

// Tests that the monochrome app icon is used on the home tab and it is
// correctly colored.
IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, MonochromeAppIconOnHomeTab) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDefaultTheme();

  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_fixed_home_scope.json");

  base::RunLoop run_loop;
  WebAppProvider::GetForTest(browser()->profile())
      ->icon_manager()
      .SetFaviconMonochromeReadCallbackForTesting(base::BindLambdaForTesting(
          [&](const webapps::AppId& cached_app_id) { run_loop.Quit(); }));

  webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
  run_loop.Run();

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  TabIcon* tab_icon = BrowserView::GetBrowserViewForBrowser(app_browser)
                          ->tabstrip()
                          ->tab_at(0)
                          ->GetTabIconForTesting();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->active_index(), 0);

  const SkBitmap* favicon_bitmap = tab_icon->GetThemedIconForTesting().bitmap();

  EXPECT_EQ(favicon_bitmap->width(), 16);
  EXPECT_EQ(favicon_bitmap->height(), 16);
  EXPECT_EQ(gfx::kGoogleGrey900, favicon_bitmap->getColor(0, 0));

  // Adding a new tab causes the home tab to be inactive and colored black (the
  // app theme_color), so the icon should change color.
  chrome::NewTab(app_browser);

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  favicon_bitmap = tab_icon->GetThemedIconForTesting().bitmap();

  EXPECT_EQ(favicon_bitmap->width(), 16);
  EXPECT_EQ(favicon_bitmap->height(), 16);
  EXPECT_EQ(SK_ColorWHITE, favicon_bitmap->getColor(0, 0));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, InstallFromNonHomeTabUrl) {
  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_customizations.json");
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(install_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect the home tab was opened in addition to the install URL.
  EXPECT_EQ(tab_strip->count(), 2);
  // Home tab - start URL.
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  // Regular tab (active) - install URL.
  EXPECT_FALSE(tab_strip->IsTabPinned(1));
  EXPECT_EQ(tab_strip->active_index(), 1);
  EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetVisibleURL(), install_url);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, OpeningPinsHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  // Install and close app.
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  CloseAndWait(app_browser);

  // Launch the app to a non home tab URL.
  app_browser = LaunchWebAppToURL(
      browser()->profile(), app_id,
      embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect the home tab was opened in addition to the launch URL.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Open app at home tab URL.
  EXPECT_EQ(app_browser,
            LaunchWebAppToURL(browser()->profile(), app_id, start_url));

  // Expect the home tab to be focused.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, ReparentingPinsHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  // Install and close app.
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  CloseAndWait(app_browser);

  // Navigate to the app URL in the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/web_apps/favicon_only.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Reparent web contents into app browser.
  app_browser =
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  // Expect the pinned home tab to also be opened.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to home tab URL in the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/web_apps/tab_strip_customizations.html")));
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Reparent web contents into app browser.
  EXPECT_EQ(app_browser,
            web_app::ReparentWebContentsIntoAppBrowser(web_contents, app_id));
  tab_strip = app_browser->tab_strip_model();

  // Expect home tab to be focused.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, NavigationThrottle) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Navigate to a non home tab URL.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));

  // Expect URL to have opened in new tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetVisibleURL(),
            embedded_test_server()->GetURL("/web_apps/get_manifest.html"));

  // Navigate to home tab URL with hash ref.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL(
                     "/web_apps/tab_strip_customizations.html#some_hash"));

  // Expect navigation to happen in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/tab_strip_customizations.html#some_hash"));

  // Navigate to a non home tab URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/get_manifest.html?blah"));

  // Expect URL to have opened in new tab.
  EXPECT_EQ(tab_strip->count(), 3);
  EXPECT_EQ(tab_strip->active_index(), 1);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/tab_strip_customizations.html#some_hash"));
  EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetVisibleURL(),
            embedded_test_server()->GetURL("/web_apps/get_manifest.html?blah"));
  EXPECT_EQ(tab_strip->GetWebContentsAt(2)->GetVisibleURL(),
            embedded_test_server()->GetURL("/web_apps/get_manifest.html"));

  // Navigate to home tab URL.
  OpenUrlAndWait(app_browser, start_url);

  // Expect navigation to happen in home tab.
  EXPECT_EQ(tab_strip->count(), 3);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
}

// TODO(crbug.com/40257354): Enable this test.
IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, DISABLED_TargetBlankLink) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Open a second tab then refocus the home tab.
  chrome::NewTab(app_browser);
  tab_strip->ActivateTabAt(0);

  // Navigate to a home tab URL via a target=_blank link.
  content::TestNavigationObserver nav_observer(
      tab_strip->GetActiveWebContents(), 1);
  ASSERT_TRUE(ExecJs(
      tab_strip->GetActiveWebContents(),
      "document.getElementById('test-link-with-blank-target').click();"));
  nav_observer.Wait();

  // Expect no new tab was opened, and the home tab is focused.
  EXPECT_EQ(tab_strip->count(), 3);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/tab_strip_customizations.html#target_blank"));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, OpenInChrome) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // 'Open in Chrome' menu item should not be enabled for the pinned home tab.
  EXPECT_FALSE(
      app_browser->command_controller()->IsCommandEnabled(IDC_OPEN_IN_CHROME));

  chrome::NewTab(app_browser);
  // 'Open in Chrome' menu item should be enabled for other tabs.
  EXPECT_TRUE(
      app_browser->command_controller()->IsCommandEnabled(IDC_OPEN_IN_CHROME));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, WebAppMenuModelTabbedApp) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  WebAppMenuModel model(nullptr, app_browser);
  model.Init();
  // Check menu contains 'New Tab'.
  EXPECT_TRUE(model.GetIndexOfCommandId(IDC_NEW_TAB).has_value());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, WebAppMenuModelNonTabbedApp) {
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id = InstallTestWebApp(start_url, /*await_metric=*/false);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  WebAppMenuModel model(nullptr, app_browser);
  model.Init();
  // Check menu does not contain 'New Tab'.
  EXPECT_FALSE(model.GetIndexOfCommandId(IDC_NEW_TAB).has_value());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, MoveTabsToNewWindow) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ui_test_utils::WaitUntilBrowserBecomeActive(app_browser);

  chrome::NewTab(app_browser);

  size_t initial_browser_count = BrowserList::GetInstance()->size();

  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::MoveTabsToNewWindow(app_browser, {1});
  Browser* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

  EXPECT_EQ(initial_browser_count + 1, BrowserList::GetInstance()->size());

  // Check that the tab made it to a new window.
  EXPECT_NE(app_browser, new_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(new_browser, app_id));
  EXPECT_EQ(app_browser->tab_strip_model()->count(), 1);

  // Check the new browser contains the moved tab and a pinned home tab.
  EXPECT_EQ(new_browser->tab_strip_model()->count(), 2);
  EXPECT_TRUE(new_browser->tab_strip_model()->IsTabPinned(0));
  EXPECT_EQ(
      new_browser->tab_strip_model()->GetWebContentsAt(0)->GetVisibleURL(),
      start_url);
  EXPECT_EQ(new_browser->tab_strip_model()->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, MoveTabsToExistingWindow) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ui_test_utils::WaitUntilBrowserBecomeActive(app_browser);
  chrome::NewTab(app_browser);

  // Open a second app browser window.
  ui_test_utils::BrowserChangeObserver app_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::MoveTabsToNewWindow(app_browser, {1});
  Browser* app_browser2 = app_browser_observer.Wait();
  ASSERT_TRUE(app_browser2);
  ui_test_utils::WaitUntilBrowserBecomeActive(app_browser2);

  EXPECT_EQ(app_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(app_browser2->tab_strip_model()->count(), 2);

  // Test the "open in existing window" menu option.
  TabMenuModel menu(nullptr, app_browser2->tab_menu_model_delegate(),
                    app_browser2->tab_strip_model(), 1);
  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandMoveToExistingWindow)
          .value();
  ExistingWindowSubMenuModel* submenu =
      static_cast<ExistingWindowSubMenuModel*>(
          menu.GetSubmenuModelAt(submenu_index));
  EXPECT_EQ(submenu->GetItemCount(), 3u);
  EXPECT_EQ(submenu->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_TAB_CXMENU_MOVETOANOTHERNEWWINDOW));
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetLabelAt(2), app_browser->GetWindowTitleForCurrentTab(
                                        /*include_app_name=*/false));

  submenu->ExecuteCommand(submenu->GetCommandIdAt(2), 0);

  EXPECT_EQ(app_browser2->tab_strip_model()->count(), 1);
  EXPECT_EQ(app_browser->tab_strip_model()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       OnlyNavigateHomeTabIfDifferentUrl) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Execute some JS to set a variable.
  EXPECT_EQ(nullptr, EvalJs(tab_strip->GetWebContentsAt(0), "var test = 5"));

  // Navigate to a non home tab URL.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to home tab using the same URL.
  app_browser->OpenURL(OpenURLParams(start_url, content::Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});

  // Expect the JS variable to still be set, meaning the page was not navigated.
  EXPECT_EQ(true, EvalJs(tab_strip->GetWebContentsAt(0), "test == 5"));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Launch the app to the home tab using the same URL.
  app_browser = LaunchWebAppToURL(browser()->profile(), app_id, start_url);

  // Expect the JS variable to still be set, meaning the page was not navigated.
  EXPECT_EQ(true, EvalJs(tab_strip->GetWebContentsAt(0), "test == 5"));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, NoFavicons) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // No favicons shown for web apps.
  EXPECT_FALSE(tab_strip->delegate()->ShouldDisplayFavicon(
      tab_strip->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       OnlyThrottlePrimaryMainFrame) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  content::WebContents* web_contents = tab_strip->GetActiveWebContents();

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  GURL iframe_url = embedded_test_server()->GetURL("/iframe_blank.html");
  GURL iframe_nav_url =
      embedded_test_server()->GetURL("/web_apps/get_manifest.html");

  content::TestNavigationObserver nav_observer(web_contents, 1);
  EXPECT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", iframe_nav_url));
  nav_observer.Wait();

  // Expect the navigation happened in the iframe and no new tab was opened.
  EXPECT_EQ(iframe_nav_url, nav_observer.last_navigation_url());
  EXPECT_EQ(tab_strip->count(), 1);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, DontCreateThrottleForReload) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Reload.
  content::TestNavigationObserver reload_nav_observer(
      tab_strip->GetActiveWebContents(), 1);
  chrome::Reload(app_browser, WindowOpenDisposition::CURRENT_TAB);
  reload_nav_observer.Wait();

  // Expect the reload did not cause a new tab to open.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, QueryParamsInStartUrl) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_query_params_in_start_url.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect only the home tab was opened.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);

  // Navigate to start_url with different query params.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/get_manifest.html?query=param"));

  // Expect navigation to happen in a new tab in that window.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->active_index(), 1);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/get_manifest.html?query=param"));

  // Navigate to start_url.
  GURL start_url_with_fragment = embedded_test_server()->GetURL(
      "/web_apps/"
      "get_manifest.html?tab_strip_query_params_in_start_url.json#param");
  OpenUrlAndWait(app_browser, start_url_with_fragment);

  // Expect that the home tab is focused and the new tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            start_url_with_fragment);
  EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/get_manifest.html?query=param"));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       OutOfScopeNavigationFromHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Navigate to an out of scope URL.
  OpenUrlAndWait(app_browser, GURL("https://www.example.com"));

  // Expect URL to have opened in a new browser tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL("https://www.example.com"));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, TabbedModeMediaCSS) {
  GURL start_url = embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_tabbed_display_override.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  std::string match_media_standalone =
      "window.matchMedia('(display-mode: standalone)').matches;";
  std::string match_media_tabbed =
      "window.matchMedia('(display-mode: tabbed)').matches;";
  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));
  ASSERT_FALSE(EvalJs(web_contents, match_media_standalone).ExtractBool());
  ASSERT_TRUE(EvalJs(web_contents, match_media_tabbed).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       CloseTabCommandDisabledForHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  chrome::BrowserCommandController* commandController =
      app_browser->command_controller();
  // Close tab command should be enabled since the home tab is the only tab.
  EXPECT_TRUE(commandController->IsCommandEnabled(IDC_CLOSE_TAB));

  // Open a new tab.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  EXPECT_EQ(tab_strip->count(), 2);

  // Close tab command should be enabled since this is not the home tab.
  EXPECT_TRUE(commandController->IsCommandEnabled(IDC_CLOSE_TAB));

  tab_strip->ActivateTabAt(0);

  // Close tab command should not be enabled for home tab when there are
  // multiple tabs open.
  EXPECT_FALSE(commandController->IsCommandEnabled(IDC_CLOSE_TAB));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       MiddleClickDoesntCloseHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip_model = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip_model->count(), 1);
  EXPECT_TRUE(tab_strip_model->IsTabPinned(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip_model->active_index(), 0);

  BrowserView* view = BrowserView::GetBrowserViewForBrowser(app_browser);

  // Open another tab.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  EXPECT_EQ(tab_strip_model->count(), 2);

  // Home tab should not be closable.
  view->tabstrip()->CloseTab(view->tabstrip()->tab_at(0),
                             CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(tab_strip_model->count(), 2);

  // Non home tab should be closable.
  view->tabstrip()->CloseTab(view->tabstrip()->tab_at(1),
                             CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(tab_strip_model->count(), 1);

  // The home tab is the only tab open so it can be closed.
  view->tabstrip()->CloseTab(view->tabstrip()->tab_at(0),
                             CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(tab_strip_model->count(), 0);
}

// Tests that the home tab is not closable unless it is the only tab left in the
// window. This is to ensure that window.close() does not close the home tab.
IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, HomeTabCantBeClosedUsingJS) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  UnloadController unload_controller(app_browser);

  // Check that the home tab can be closed since it is the only tab in the
  // window.
  EXPECT_TRUE(
      unload_controller.CanCloseContents(tab_strip->GetWebContentsAt(0)));

  // Open another tab.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  EXPECT_EQ(tab_strip->count(), 2);

  // Check that the home tab can't be closed.
  EXPECT_FALSE(
      unload_controller.CanCloseContents(tab_strip->GetWebContentsAt(0)));

  // Check that non home tab is closable.
  EXPECT_TRUE(
      unload_controller.CanCloseContents(tab_strip->GetWebContentsAt(1)));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, HomeTabScopeSegmentWildcard) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
  // Navigate to an out of home tab scope URL.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
  // Expect URL to have opened in a new  tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to an in home tab scope URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/title_appname_prefix.html"));
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(
      tab_strip->GetActiveWebContents()->GetVisibleURL(),
      embedded_test_server()->GetURL("/web_apps/title_appname_prefix.html"));

  // Navigate to start_url.
  OpenUrlAndWait(app_browser, start_url);
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(), start_url);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, HomeTabScopeFixedString) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_fixed_home_scope.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Navigate to an in home tab scope URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/title_appname_prefix.html"));
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(
      tab_strip->GetActiveWebContents()->GetVisibleURL(),
      embedded_test_server()->GetURL("/web_apps/title_appname_prefix.html"));

  // Navigate to an out of home tab scope URL.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
  // Expect URL to have opened in a new  tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to another in home tab scope URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/tab_strip_customizations.html"));
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/web_apps/tab_strip_customizations.html"));

  // Navigate to start_url.
  OpenUrlAndWait(app_browser, start_url);
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(), start_url);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, HomeTabScopeWildcardString) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_wildcard_home_scope.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Navigate to an in home tab scope URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/title_appname_prefix.html"));
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(
      tab_strip->GetActiveWebContents()->GetVisibleURL(),
      embedded_test_server()->GetURL("/web_apps/title_appname_prefix.html"));

  // Navigate to an out of home tab scope URL.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/banners/theme-color.html"));
  // Expect URL to have opened in a new  tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to another in home tab scope URL.
  OpenUrlAndWait(app_browser, embedded_test_server()->GetURL(
                                  "/web_apps/standalone/basic.html"));
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(),
            embedded_test_server()->GetURL("/web_apps/standalone/basic.html"));

  // Navigate to start_url.
  OpenUrlAndWait(app_browser, start_url);
  // Expect it was opened in home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(), start_url);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, CloseAllTabsCommand) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  chrome::NewTab(app_browser);
  chrome::NewTab(app_browser);
  chrome::NewTab(app_browser);

  EXPECT_EQ(tab_strip->count(), 4);

  tab_strip->ExecuteContextMenuCommand(
      2, TabStripModel::ContextMenuCommand::CommandCloseAllTabs);

  // Expect all tabs closed except the home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       NewTabButtonUrlInHomeTabScope) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_wildcard_home_scope.json");
  webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  EXPECT_TRUE(app_browser->app_controller()->ShouldHideNewTabButton());

  WebAppMenuModel model(nullptr, app_browser);
  model.Init();
  // Check menu does not contain 'New Tab'.
  EXPECT_FALSE(model.GetIndexOfCommandId(IDC_NEW_TAB).has_value());
}

// Tests that middle clicking links to the home tab, opens the link in the home
// tab.
IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, MiddleClickHomeTabLink) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Middle click home tab link from the home tab.
  app_browser->OpenURL(OpenURLParams(start_url, content::Referrer(),
                                     WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});

  // Check we stayed in the home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  chrome::NewTab(app_browser);

  // Middle click home tab link from a different tab.
  app_browser->OpenURL(OpenURLParams(start_url, content::Referrer(),
                                     WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});

  // Check it was opened in the home tab.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

// Tests the page title, which is used for accessibility.
IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, PageTitle) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  webapps::AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect app opened with pinned home tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);

  // The tab title starts with the tab name, followed by whether it is pinned
  // but may also have more things after that.
  EXPECT_TRUE(base::StartsWith(browser_view->GetAccessibleTabLabel(0),
                               u"Tab Strip Customizations - Pinned"));

  chrome::NewTab(app_browser);
  content::WaitForLoadStop(tab_strip->GetActiveWebContents());

  EXPECT_TRUE(base::StartsWith(browser_view->GetAccessibleTabLabel(0),
                               u"Tab Strip Customizations - Pinned"));
  EXPECT_TRUE(base::StartsWith(browser_view->GetAccessibleTabLabel(1),
                               u"Favicon only"));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Browser tests that verify web-app tab strip behavior when locked (and not
// locked) for OnTask. Only relevant for non-web browser scenarios.
using WebAppTabStripForOnTaskBrowserTest = WebAppTabStripBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppTabStripForOnTaskBrowserTest,
                       MiddleClickDoesNotCloseTabsWhenLockedForOnTask) {
  // Set up app and lock the app for OnTask.
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  const webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* const app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(true);

  const TabStripModel* const tab_strip_model = app_browser->tab_strip_model();
  ASSERT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));
  ASSERT_EQ(tab_strip_model->count(), 1);
  ASSERT_TRUE(tab_strip_model->IsTabPinned(0));

  // Open another tab so we can test tab close behavior on both home and
  // non-home tabs.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Verify home tab cannot be closed.
  auto* const tab_strip =
      BrowserView::GetBrowserViewForBrowser(app_browser)->tabstrip();
  tab_strip->CloseTab(tab_strip->tab_at(0),
                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Also verify the non-home tab cannot be closed.
  tab_strip->CloseTab(tab_strip->tab_at(1),
                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(tab_strip_model->count(), 2);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripForOnTaskBrowserTest,
                       MiddleClickCanCloseTabsWhenNotLockedForOnTask) {
  // Set up app and do not lock the app for OnTask.
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  const webapps::AppId app_id = InstallTestWebApp(start_url);
  Browser* const app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(false);

  const TabStripModel* const tab_strip_model = app_browser->tab_strip_model();
  ASSERT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));
  ASSERT_EQ(tab_strip_model->count(), 1);
  ASSERT_TRUE(tab_strip_model->IsTabPinned(0));

  // Open another tab so we can test tab close behavior on both home and
  // non-home tabs.
  OpenUrlAndWait(app_browser,
                 embedded_test_server()->GetURL("/web_apps/get_manifest.html"));
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Verify home tab cannot be closed (default behavior on tabbed web apps).
  auto* const tab_strip =
      BrowserView::GetBrowserViewForBrowser(app_browser)->tabstrip();
  tab_strip->CloseTab(tab_strip->tab_at(0),
                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Verify the non-home tab can be closed.
  tab_strip->CloseTab(tab_strip->tab_at(1),
                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  ASSERT_EQ(tab_strip_model->count(), 1);

  // The home tab is the only tab open so it can be closed now.
  tab_strip->CloseTab(tab_strip->tab_at(0),
                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(tab_strip_model->count(), 0);
}

#endif

}  // namespace web_app
