// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

using content::RenderFrameHost;
using content::WebContents;
using content::test::PrerenderHostObserver;
using content::test::PrerenderHostRegistryObserver;
using content::test::PrerenderTestHelper;
using ui_test_utils::BrowserChangeObserver;

namespace web_app {
namespace {
using ClientMode = LaunchHandler::ClientMode;

// Tests that links are captured correctly into an installed WebApp using the
// 'tabbed' display mode, which allows the webapp window to have multiple tabs.
class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() {
    std::vector<base::test::FeatureRef> features = {
        blink::features::kWebAppEnableLaunchHandler};
#if !BUILDFLAG(IS_CHROMEOS)
    features.push_back(apps::features::kDesktopPWAsLinkCapturing);
#endif
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        features,
        /*disabled_features=*/{});
  }
  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server().Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    out_of_scope_ = https_server().GetURL("/");
  }

  // Returns [app_id, in_scope_1, in_scope_2, scope]
  std::tuple<webapps::AppId, GURL, GURL, GURL> InstallTestApp(
      const char* path) {
    GURL start_url = embedded_test_server()->GetURL(path);
    GURL in_scope_1 = start_url.Resolve("page1.html");
    GURL in_scope_2 = start_url.Resolve("page2.html");
    GURL scope = start_url.GetWithoutFilename();

    webapps::AppId app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return std::make_tuple(app_id, in_scope_1, in_scope_2, scope);
  }

  GURL GetNestedAppUrl() {
    return embedded_test_server()->GetURL(
        "/web_apps/nesting/nested/index.html");
  }

  GURL GetParentAppUrl() {
    return embedded_test_server()->GetURL("/web_apps/nesting/index.html");
  }

  webapps::AppId InstallParentApp() {
    GURL start_url = GetParentAppUrl();
    webapps::AppId app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  webapps::AppId InstallNestedApp() {
    GURL start_url = GetNestedAppUrl();
    webapps::AppId app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url);
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  WebAppProvider& provider() {
    auto* provider = WebAppProvider::GetForTest(profile());
    DCHECK(provider);
    return *provider;
  }

  void AddTab(Browser* browser, const GURL& url) {
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser, url, /*index=*/-1, /*foreground=*/true);
    observer.Wait();
  }

  void Navigate(Browser* browser,
                const GURL& url,
                LinkTarget link_target = LinkTarget::SELF) {
    ClickLinkAndWait(browser->tab_strip_model()->GetActiveWebContents(), url,
                     link_target, "");
  }

  Browser* GetNewBrowserFromNavigation(Browser* browser,
                                       const GURL& url,
                                       bool preserve_about_blank = true) {
    if (preserve_about_blank && browser->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetVisibleURL()
                                    .IsAboutBlank()) {
      // Create a new tab to link capture in because about:blank tabs are
      // destroyed after link capturing, see:
      // CommonAppsNavigationThrottle::ShouldCancelNavigation()
      AddTab(browser, about_blank_);
    }

    BrowserChangeObserver observer(nullptr,
                                   BrowserChangeObserver::ChangeType::kAdded);
    Navigate(browser, url);
    return observer.Wait();
  }

  void ExpectTabs(Browser* test_browser,
                  std::vector<GURL> urls,
                  base::Location location = FROM_HERE) {
    std::string debug_info = "\nOpen browsers:\n";
    for (Browser* open_browser : *BrowserList::GetInstance()) {
      debug_info += "  ";
      if (open_browser == browser()) {
        debug_info += "Main browser";
      } else if (open_browser->app_controller()) {
        debug_info += "App browser";
      } else {
        debug_info += "Browser";
      }
      debug_info += ":\n";
      for (int i = 0; i < open_browser->tab_strip_model()->count(); ++i) {
        debug_info += "   - " +
                      open_browser->tab_strip_model()
                          ->GetWebContentsAt(i)
                          ->GetVisibleURL()
                          .spec() +
                      "\n";
      }
    }
    SCOPED_TRACE(location.ToString());
    SCOPED_TRACE(debug_info);
    TabStripModel& tab_strip = *test_browser->tab_strip_model();
    ASSERT_EQ(static_cast<size_t>(tab_strip.count()), urls.size());
    for (int i = 0; i < tab_strip.count(); ++i) {
      SCOPED_TRACE(base::StringPrintf("is app browser: %d, tab index: %d",
                                      bool(test_browser->app_controller()), i));
      EXPECT_EQ(
          test_browser->tab_strip_model()->GetWebContentsAt(i)->GetVisibleURL(),
          urls[i]);
    }
  }

  void TurnOnLinkCapturing(webapps::AppId app_id) {
#if BUILDFLAG(IS_CHROMEOS)
    apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_id);
#else
    base::test::TestFuture<void> preference_set;
    provider().scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app_id, true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  absl::optional<LaunchHandler> GetLaunchHandler(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id)->launch_handler();
  }

 protected:
  GURL out_of_scope_;

  const GURL about_blank_{"about:blank"};

  base::test::ScopedFeatureList feature_list_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

// Link capturing with navigate_existing_client: always should navigate existing
// app windows.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       NavigateExistingClientFromBrowser) {
  const auto [app_id, in_scope_1, _, scope] = InstallTestApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateExisting}));

  TurnOnLinkCapturing(app_id);

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1});

  // Navigate the app window out of scope to ensure the captured link triggers a
  // navigation.
  Navigate(app_browser, out_of_scope_);
  ExpectTabs(app_browser, {out_of_scope_});

  // Click a link in the browser in to scope. Ensure that no additional tabs get
  // opened in the browser.
  Navigate(browser(), in_scope_1);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1});
}

// Link captures from about:blank cleans up the about:blank page.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       AboutBlankNavigationCleanUp) {
  const auto [app_id, in_scope_1, _, scope] =
      InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing(app_id);

  ExpectTabs(browser(), {about_blank_});
  BrowserChangeObserver removed_observer(
      browser(), BrowserChangeObserver::ChangeType::kRemoved);

  // Navigate an about:blank page.
  Browser* app_browser = GetNewBrowserFromNavigation(
      browser(), in_scope_1, /*preserve_about_blank=*/false);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  ExpectTabs(app_browser, {in_scope_1});

  // Old about:blank page cleaned up.
  removed_observer.Wait();
}

// JavaScript initiated link captures from about:blank cleans up the about:blank
// page.
// TODO(https://crbug.com/1497363): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_JavascriptAboutBlankNavigationCleanUp \
  DISABLED_JavascriptAboutBlankNavigationCleanUp
#else
#define MAYBE_JavascriptAboutBlankNavigationCleanUp \
  JavascriptAboutBlankNavigationCleanUp
#endif
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       MAYBE_JavascriptAboutBlankNavigationCleanUp) {
  const auto [app_id, in_scope_1, _, scope] =
      InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing(app_id);

  ExpectTabs(browser(), {about_blank_});
  BrowserChangeObserver removed_observer(
      browser(), BrowserChangeObserver::ChangeType::kRemoved);

  // Navigate an about:blank page using JavaScript.
  BrowserChangeObserver added_observer(
      nullptr, BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("location = '%s';", in_scope_1.spec().c_str())));
  Browser* app_browser = added_observer.Wait();
  ExpectTabs(app_browser, {in_scope_1});

  // Old about:blank page cleaned up.
  removed_observer.Wait();

  // Must wait for link capturing launch to complete so that its keep alives go
  // out of scope.
  base::test::TestFuture<void> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       DifferentPortConsideredDifferent) {
  net::EmbeddedTestServer other_server;
  other_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(other_server.Start());
  ASSERT_EQ(embedded_test_server()->GetOrigin().scheme(),
            other_server.GetOrigin().scheme());
  ASSERT_EQ(embedded_test_server()->GetOrigin().host(),
            other_server.GetOrigin().host());
  ASSERT_NE(embedded_test_server()->GetOrigin().port(),
            other_server.GetOrigin().port());

  const auto [app_id, url1, url2, scope] =
      InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing(app_id);

  ExpectTabs(browser(), {about_blank_});
  GURL url = other_server.GetURL("/web_apps/basic.html");
  Navigate(browser(), url);
  ExpectTabs(browser(), {url});
}

// Tests that links to apps from sandboxed iframes can be captured.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       HandleClickFromSandboxedIframe) {
  const auto [app_id, in_scope_1, _, scope] =
      InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing(app_id);

  // Create a sandboxed iframe, which contains a link to the installed app,
  // covering the full viewport of the iframe.
  constexpr char kIframeCaptureJs[] = R"js(
    (() => {
      let i = document.createElement("iframe");
      i.id = 'iframe';
      i.sandbox = "allow-popups allow-popups-to-escape-sandbox";
      i.srcdoc = `<a href="$1"
          target="_blank"
          style="position: absolute; top: 0; left: 0; bottom: 0; right: 0;">
        </a>`
      document.body.appendChild(i);
    })();
  )js";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      base::ReplaceStringPlaceholders(kIframeCaptureJs, {in_scope_1.spec()},
                                      /*offsets=*/nullptr)));

  BrowserChangeObserver added_observer(
      nullptr, BrowserChangeObserver::ChangeType::kAdded);

  // Click the iframe, which should click the <a> tag and open the app.
  content::SimulateMouseClickOrTapElementWithId(web_contents, "iframe");

  Browser* app_browser = added_observer.Wait();
  EXPECT_NE(browser(), app_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       ParentAppWithChildLinks) {
  webapps::AppId parent_app_id = InstallParentApp();
  webapps::AppId nested_app_id = InstallNestedApp();

  TurnOnLinkCapturing(parent_app_id);
  AddTab(browser(), about_blank_);

  BrowserChangeObserver added_observer(
      nullptr, BrowserChangeObserver::ChangeType::kAdded);

  Navigate(browser(), GetNestedAppUrl());

  // https://crbug.com/1476011: ChromeOS currently capturing nested app links
  // into the parent app, but other platforms split the URL space and fully
  // respect the child app's user setting.
#if BUILDFLAG(IS_CHROMEOS)
  Browser* app_browser = added_observer.Wait();
  EXPECT_NE(browser(), app_browser);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, parent_app_id));
  ExpectTabs(app_browser, {GetNestedAppUrl()});
  ExpectTabs(browser(), {about_blank_});
#else
  ExpectTabs(browser(), {about_blank_, GetNestedAppUrl()});
#endif
}

// https://crbug.com/1476011: ChromeOS currently capturing nested app links into
// the parent app, treating them as overlapping apps. Other platforms split the
// URL space and fully respect the child app's user setting.
// Thus, on non-CrOS platforms both apps can capture links.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       ParentAppAndChildAppCapture) {
  webapps::AppId parent_app_id = InstallParentApp();
  webapps::AppId nested_app_id = InstallNestedApp();

  TurnOnLinkCapturing(parent_app_id);
  TurnOnLinkCapturing(nested_app_id);

  Browser* nested_browser;
  Browser* parent_browser;
  {
    BrowserChangeObserver added_observer(
        nullptr, BrowserChangeObserver::ChangeType::kAdded);
    // Add a tab to prevent the browser closing.
    AddTab(browser(), about_blank_);
    Navigate(browser(), GetNestedAppUrl());
    nested_browser = added_observer.Wait();
  }
  {
    BrowserChangeObserver added_observer(
        nullptr, BrowserChangeObserver::ChangeType::kAdded);
    // Add a tab to prevent the browser closing.
    AddTab(browser(), about_blank_);
    Navigate(browser(), GetParentAppUrl());
    parent_browser = added_observer.Wait();
  }
  ASSERT_TRUE(nested_browser);
  ASSERT_TRUE(parent_browser);

  EXPECT_NE(browser(), nested_browser);
  EXPECT_NE(browser(), parent_browser);
  EXPECT_NE(nested_browser, parent_browser);

  EXPECT_TRUE(AppBrowserController::IsForWebApp(nested_browser, nested_app_id));
  EXPECT_TRUE(AppBrowserController::IsForWebApp(parent_browser, parent_app_id));

  ExpectTabs(browser(), {about_blank_});
  ExpectTabs(nested_browser, {GetNestedAppUrl()});
  ExpectTabs(parent_browser, {GetParentAppUrl()});
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Tests that link capturing works while inside a web app window.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       LinkCaptureInWebAppWindow) {
  webapps::AppId parent_app_id = InstallParentApp();
  webapps::AppId nested_app_id = InstallNestedApp();

  TurnOnLinkCapturing(nested_app_id);

  content::WebContents* parent_app = OpenApplication(parent_app_id);

  // Clicking a link from target="_self" should link capture.
  {
    BrowserChangeObserver added_observer(
        nullptr, BrowserChangeObserver::ChangeType::kAdded);
    ClickLinkAndWait(parent_app, GetNestedAppUrl(), LinkTarget::SELF,
                     /*rel=*/"");
    EXPECT_TRUE(AppBrowserController::IsForWebApp(added_observer.Wait(),
                                                  nested_app_id));
  }

  // Clicking a link from target="_blank" should also link capture.
  {
    BrowserChangeObserver added_observer(
        nullptr, BrowserChangeObserver::ChangeType::kAdded);
    ClickLinkAndWait(parent_app, GetNestedAppUrl(), LinkTarget::BLANK,
                     /*rel=*/"");
    EXPECT_TRUE(AppBrowserController::IsForWebApp(added_observer.Wait(),
                                                  nested_app_id));
  }

  // Links clicked within an app popup browser will also capture.
  {
    const gfx::Size size(500, 500);
    Browser* const popup_browser = OpenPopupAndWait(
        chrome::FindBrowserWithTab(parent_app), GetParentAppUrl(), size);

    BrowserChangeObserver added_observer(
        nullptr, BrowserChangeObserver::ChangeType::kAdded);
    ClickLinkAndWait(popup_browser->tab_strip_model()->GetActiveWebContents(),
                     GetNestedAppUrl(), LinkTarget::SELF,
                     /*rel=*/"");
    EXPECT_TRUE(AppBrowserController::IsForWebApp(added_observer.Wait(),
                                                  nested_app_id));
  }
}

IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       NoLinkCaptureOutOfScopeInAppWindow) {
  const auto [app_id, in_scope_1, _, scope] =
      InstallTestApp("/web_apps/basic.html");

  TurnOnLinkCapturing(app_id);

  content::WebContents* test_app = OpenApplication(app_id);

  ClickLinkAndWait(test_app, out_of_scope_, LinkTarget::SELF, /*rel=*/"");

  ClickLinkAndWait(test_app, in_scope_1, LinkTarget::SELF, /*rel=*/"");

  ExpectTabs(chrome::FindBrowserWithTab(test_app), {in_scope_1});
}

// TODO: Run these tests on Chrome OS with both Ash and Lacros processes active.
class WebAppTabStripLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest {
 public:
  WebAppTabStripLinkCapturingBrowserTest() {
    std::vector<base::test::FeatureRef> features = {
        blink::features::kDesktopPWAsTabStrip,
        features::kDesktopPWAsTabStripSettings};
#if !BUILDFLAG(IS_CHROMEOS)
    features.push_back(apps::features::kDesktopPWAsLinkCapturing);
#endif
    features_.InitWithFeatures(
        /*enabled_features=*/features,
        /*disabled_features=*/{});
  }

  // Returns [app_id, in_scope_1, in_scope_2, scope]
  std::tuple<webapps::AppId, GURL, GURL, GURL> InstallTestTabbedApp() {
    const auto [app_id, in_scope_1, in_scope_2, scope] =
        WebAppLinkCapturingBrowserTest::InstallTestApp("/web_apps/basic.html");
    provider().sync_bridge_unsafe().SetAppUserDisplayMode(
        app_id, mojom::UserDisplayMode::kTabbed, /*is_user_action=*/false);
    return std::make_tuple(app_id, in_scope_1, in_scope_2, scope);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// First in scope navigation from out of scope gets captured and reparented into
// the app window.
IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  const auto [app_id, in_scope_1, in_scope_2, scope] = InstallTestTabbedApp();
  TurnOnLinkCapturing(app_id);

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1});

  // Another in scope navigation should open a new tab in the same app window.
  Navigate(browser(), in_scope_2);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1, in_scope_2});

  // Whole origin should count as in scope.
  Navigate(browser(), scope);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1, in_scope_2, scope});

  // Middle clicking links should not be captured.
  ClickLinkWithModifiersAndWaitForURL(
      browser()->tab_strip_model()->GetActiveWebContents(), scope, scope,
      LinkTarget::SELF, "", blink::WebInputEvent::Modifiers::kNoModifiers,
      blink::WebMouseEvent::Button::kMiddle);
  ExpectTabs(browser(), {out_of_scope_, scope});
  ExpectTabs(app_browser, {in_scope_1, in_scope_2, scope});

  // Out of scope should behave as usual.
  Navigate(browser(), out_of_scope_);
  ExpectTabs(browser(), {out_of_scope_, scope});
  ExpectTabs(app_browser, {in_scope_1, in_scope_2, scope});
}

}  // namespace
}  // namespace web_app
