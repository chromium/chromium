// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
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

using ClientMode = LaunchHandler::ClientMode;

#if BUILDFLAG(IS_CHROMEOS)

// Tests that links are captured correctly into an installed WebApp using the
// 'tabbed' display mode, which allows the webapp window to have multiple tabs.
class WebAppLinkCapturingBrowserTest : public WebAppNavigationBrowserTest {
 public:
  WebAppLinkCapturingBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableLaunchHandler);
  }
  ~WebAppLinkCapturingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppNavigationBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server().Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    out_of_scope_ = https_server().GetURL("/");
  }

  void InstallTestApp(const char* path) {
    start_url_ = embedded_test_server()->GetURL(path);
    in_scope_1_ = start_url_.Resolve("page1.html");
    in_scope_2_ = start_url_.Resolve("page2.html");
    scope_ = start_url_.GetWithoutFilename();

    app_id_ = InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url_);
    AppReadinessWaiter(profile(), app_id_).Await();
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

  void ExpectTabs(Browser* test_browser, std::vector<GURL> urls) {
    std::string debug_info = "\nOpen browsers:\n";
    for (Browser* open_browser : *BrowserList::GetInstance()) {
      debug_info += "  ";
      if (open_browser == browser())
        debug_info += "Main browser";
      else if (open_browser->app_controller())
        debug_info += "App browser";
      else
        debug_info += "Browser";
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

  void TurnOnLinkCapturing() {
    apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_id_);
  }

  absl::optional<LaunchHandler> GetLaunchHandler(const AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id)->launch_handler();
  }

 protected:
  AppId app_id_;
  GURL start_url_;
  GURL in_scope_1_;
  GURL in_scope_2_;
  GURL scope_;
  GURL out_of_scope_;

  const GURL about_blank_{"about:blank"};

  base::test::ScopedFeatureList feature_list_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

// Link capturing with navigate_existing_client: always should navigate existing
// app windows.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       NavigateExistingClientFromBrowser) {
  InstallTestApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id_),
            (LaunchHandler{ClientMode::kNavigateExisting}));

  TurnOnLinkCapturing();

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});

  // Navigate the app window out of scope to ensure the captured link triggers a
  // navigation.
  Navigate(app_browser, out_of_scope_);
  ExpectTabs(app_browser, {out_of_scope_});

  // Click a link in the browser in to scope. Ensure that no additional tabs get
  // opened in the browser.
  Navigate(browser(), in_scope_1_);
  ExpectTabs(browser(), {out_of_scope_});
  ExpectTabs(app_browser, {in_scope_1_});
}

// Link captures from about:blank cleans up the about:blank page.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       AboutBlankNavigationCleanUp) {
  InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing();

  ExpectTabs(browser(), {about_blank_});
  BrowserChangeObserver removed_observer(
      browser(), BrowserChangeObserver::ChangeType::kRemoved);

  // Navigate an about:blank page.
  Browser* app_browser = GetNewBrowserFromNavigation(
      browser(), in_scope_1_, /*preserve_about_blank=*/false);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id_));
  ExpectTabs(app_browser, {in_scope_1_});

  // Old about:blank page cleaned up.
  removed_observer.Wait();
}

// JavaScript initiated link captures from about:blank cleans up the about:blank
// page.
IN_PROC_BROWSER_TEST_F(WebAppLinkCapturingBrowserTest,
                       JavascriptAboutBlankNavigationCleanUp) {
  InstallTestApp("/web_apps/basic.html");
  TurnOnLinkCapturing();

  ExpectTabs(browser(), {about_blank_});
  BrowserChangeObserver removed_observer(
      browser(), BrowserChangeObserver::ChangeType::kRemoved);

  // Navigate an about:blank page using JavaScript.
  BrowserChangeObserver added_observer(
      nullptr, BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("location = '%s';", in_scope_1_.spec().c_str())));
  Browser* app_browser = added_observer.Wait();
  ExpectTabs(app_browser, {in_scope_1_});

  // Old about:blank page cleaned up.
  removed_observer.Wait();

  // Must wait for link capturing launch to complete so that its keep alives go
  // out of scope.
  base::test::TestFuture<void> future;
  apps::ChromeOsAppsNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();
  ASSERT_TRUE(future.Wait());
}

// TODO: Run these tests on Chrome OS with both Ash and Lacros processes active.
class WebAppTabStripLinkCapturingBrowserTest
    : public WebAppLinkCapturingBrowserTest {
 public:
  WebAppTabStripLinkCapturingBrowserTest() {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripSettings},
                               {});
  }

  void InstallTestTabbedApp() {
    WebAppLinkCapturingBrowserTest::InstallTestApp("/web_apps/basic.html");
    provider().sync_bridge_unsafe().SetAppUserDisplayMode(
        app_id_, mojom::UserDisplayMode::kTabbed, /*is_user_action=*/false);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// First in scope navigation from out of scope gets captured and reparented into
// the app window.
IN_PROC_BROWSER_TEST_F(WebAppTabStripLinkCapturingBrowserTest,
                       InScopeNavigationsCaptured) {
  InstallTestTabbedApp();
  TurnOnLinkCapturing();

  // Start browser at an out of scope page.
  Navigate(browser(), out_of_scope_);

  // In scope navigation should open app window.
  Browser* app_browser = GetNewBrowserFromNavigation(browser(), in_scope_1_);
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

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
