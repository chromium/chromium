// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace web_app {

namespace {

constexpr char kLaunchHandlerHistogram[] =
    "Launch.WebAppLaunchHandlerClientMode";

}  // namespace

using ClientMode = LaunchHandler::ClientMode;

class WebAppLaunchHandlerBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppLaunchHandlerBrowserTest() = default;
  ~WebAppLaunchHandlerBrowserTest() override = default;

  // WebAppBrowserTestBase:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  webapps::AppId InstallTestWebApp(const char* test_file_path,
                                   bool await_metric = true) {
    BrowserWaiter browser_waiter;

    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric) {
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppManifestLaunchHandler);
    }

    webapps::AppId app_id = InstallWebAppFromPage(
        browser(), embedded_test_server()->GetURL(test_file_path));

    if (await_metric)
      metrics_waiter.Wait();

    // Installing a web app will pop it out to a new window.
    // Close this to avoid it interfering with test steps.
    Browser* app_browser = browser_waiter.AwaitAdded();
    chrome::CloseWindow(app_browser);
    browser_waiter.AwaitRemoved();

    return app_id;
  }

  const WebApp* GetWebApp(const webapps::AppId& app_id) {
    return WebAppProvider::GetForTest(profile())->registrar_unsafe().GetAppById(
        app_id);
  }

  std::optional<LaunchHandler> GetLaunchHandler(const webapps::AppId& app_id) {
    return GetWebApp(app_id)->launch_handler();
  }

  void ExpectNavigateNewBehavior(const webapps::AppId& app_id) {
    std::string start_url = GetWebApp(app_id)->start_url().spec();

    Browser* browser_1 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url);

    Browser* browser_2 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url);

    EXPECT_NE(browser_1, browser_2);
  }

  std::string AwaitNextLaunchParamsTargetUrl(Browser* browser) {
    const char* script = R"(
      new Promise(resolve => {
        window.launchQueue.setConsumer(resolve);
      }).then(params => params.targetURL);
    )";
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractString();
  }

  bool SetUpNextLaunchParamsTargetUrlPromise(Browser* browser) {
    const char* script = R"(
        window.nextLaunchParamsTargetURLPromise = new Promise(resolve => {
          window.launchQueue.setConsumer(resolve);
        }).then(params => params.targetURL);
        true;
      )";
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractBool();
  }

  std::string AwaitNextLaunchParamsTargetUrlPromise(Browser* browser) {
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                  "window.nextLaunchParamsTargetURLPromise")
        .ExtractString();
  }
};

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeEmpty) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id =
      InstallTestWebApp("/web_apps/basic.html", /*await_metric=*/false);
  EXPECT_EQ(GetLaunchHandler(app_id), std::nullopt);

  ExpectNavigateNewBehavior(app_id);

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kAuto, 2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeAuto) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?launch_handler_client_mode_auto.json");
  EXPECT_EQ(GetLaunchHandler(app_id), (LaunchHandler{ClientMode::kAuto}));

  ExpectNavigateNewBehavior(app_id);

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kAuto, 2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeNavigateNew) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_new.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateNew}));

  ExpectNavigateNewBehavior(app_id);

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kNavigateNew, 2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeNavigateExisting) {
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");

  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?"
      "client_mode=navigate-existing");

  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateExisting}));

  // Create first web app browser window.
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(app_web_contents->GetLastCommittedURL(), start_url);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(app_browser), start_url.spec());

  // Navigate window away from start_url to check that the next launch navs to
  // start_url again.
  {
    GURL alt_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(app_browser, alt_url));
    EXPECT_EQ(app_web_contents->GetLastCommittedURL(), alt_url);

    Browser* app_browser_2 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(app_browser, app_browser_2);
    EXPECT_EQ(app_web_contents->GetLastCommittedURL(), start_url);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(app_browser_2), start_url.spec());
  }

  // Reparent an in scope browser tab and check that it navigates the existing
  // web app window.
  {
    content::TestNavigationObserver observer(
        app_web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);

    chrome::NewTab(browser());
    EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
    ReparentWebAppForActiveTab(browser());
    EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

    observer.WaitForNavigationFinished();
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(app_browser), start_url.spec());
  }

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kNavigateExisting, 3);
}

// TODO(crbug.com/40219080): Fix flakiness.
IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       DISABLED_ClientModeExistingClientRetain) {
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");

  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?"
      "client_mode=focus-existing");

  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kFocusExisting}));

  Browser* browser_1 = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url.spec());

  // Navigate window away from start_url to an in scope URL, check that the
  // next launch doesn't navigate to start_url.
  {
    GURL in_scope_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    NavigateViaLinkClickToURLAndWait(browser_1, in_scope_url);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), in_scope_url);

    ASSERT_TRUE(SetUpNextLaunchParamsTargetUrlPromise(browser_1));
    Browser* browser_2 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(browser_1, browser_2);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrlPromise(browser_2),
              start_url.spec());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), in_scope_url);
  }

  // Navigate window away from start_url to an out of scope URL, check that
  // the next launch does navigate to start_url.
  {
    GURL out_of_scope_url = embedded_test_server()->GetURL("/empty.html");
    NavigateViaLinkClickToURLAndWait(browser_1, out_of_scope_url);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), out_of_scope_url);

    Browser* browser_2 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(browser_1, browser_2);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url.spec());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
  }

  // Trigger launch during navigation, check that the navigation gets
  // cancelled.
  {
    ASSERT_TRUE(
        EvalJs(web_contents, "window.thisIsTheSamePage = true").ExtractBool());

    GURL hanging_url = embedded_test_server()->GetURL("/hang");
    NavigateParams params(browser_1, hanging_url, ui::PAGE_TRANSITION_LINK);
    Navigate(&params);
    EXPECT_EQ(web_contents->GetVisibleURL(), hanging_url);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

    ASSERT_TRUE(SetUpNextLaunchParamsTargetUrlPromise(browser_1));
    Browser* browser_2 = LaunchWebAppBrowserAndWait(app_id);
    EXPECT_EQ(browser_1, browser_2);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrlPromise(browser_2),
              start_url.spec());
    EXPECT_EQ(web_contents->GetVisibleURL(), start_url);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

    // Check that we never left the current page.
    EXPECT_TRUE(EvalJs(web_contents, "window.thisIsTheSamePage").ExtractBool());
  }

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kFocusExisting, 4);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeFocusExistingMultipleLaunches) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kFocusExisting}));

  ui_test_utils::UrlLoadObserver url_observer(
      WebAppProvider::GetForTest(profile())->registrar_unsafe().GetAppLaunchUrl(
          app_id));

  // Launch the app three times in quick succession.
  Browser* browser_1 = LaunchWebAppBrowser(app_id);
  Browser* browser_2 = LaunchWebAppBrowser(app_id);
  Browser* browser_3 = LaunchWebAppBrowser(app_id);
  EXPECT_EQ(browser_1, browser_2);
  EXPECT_EQ(browser_2, browser_3);

  url_observer.Wait();
  // Wait for all launches to complete.
  WebAppProvider::GetForTest(profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  // Check that all 3 LaunchParams got enqueued.
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?client_mode=focus-existing");
  const char* script = R"(
      new Promise(resolve => {
        let remaining = 3;
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
          if (--remaining == 0) {
            resolve(targetURLs.join('|'));
          }
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(),
            base::StrCat({start_url.spec(), "|", start_url.spec(), "|",
                          start_url.spec()}));

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kFocusExisting, 3);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeNavigateExistingMultipleLaunches) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateExisting}));

  // Launch the app three times in quick succession.
  Browser* browser_1 = LaunchWebAppBrowserAndWait(app_id);
  Browser* browser_2 = LaunchWebAppBrowserAndWait(app_id);
  Browser* browser_3 = LaunchWebAppBrowserAndWait(app_id);
  EXPECT_EQ(browser_1, browser_2);
  EXPECT_EQ(browser_2, browser_3);

  // Check that only the last LaunchParams made it through.
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?client_mode=navigate-existing");
  const char* script = R"(
      new Promise(resolve => {
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
          // Wait a tick to let any additional erroneous launchParams get added.
          requestAnimationFrame(() => {
            resolve(targetURLs.join('|'));
          });
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(), start_url.spec());

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kNavigateExisting, 3);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       LaunchNavigationInterruptedByOutOfScopeNavigation) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_new.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateNew}));

  // Launch the web app and immediately navigate it out of scope during its
  // initial navigation.
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  GURL out_of_scope_url = embedded_test_server()->GetURL("/empty.html");
  NavigateViaLinkClickToURLAndWait(app_browser, out_of_scope_url);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), out_of_scope_url);

  // Check that the launch params are not enqueued in the out of scope document.
  const char* script = R"(
      new Promise(resolve => {
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
        });
        // Wait a tick to let any erroneous launch params get added.
        requestAnimationFrame(() => {
          resolve(targetURLs.join('|'));
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(), "");

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kNavigateNew, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, GlobalLaunchQueue) {
  base::HistogramTester histogram_tester;
  webapps::AppId app_id =
      InstallTestWebApp("/web_apps/basic.html", /*await_metric=*/false);

  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.launchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchParams").ExtractBool());

  histogram_tester.ExpectUniqueSample(kLaunchHandlerHistogram,
                                      ClientMode::kAuto, 1);
}

// https://crbug.com/1444959
// TODO(crbug.com/40919435): Re-enable this test
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SelectActiveBrowser DISABLED_SelectActiveBrowser
#else
#define MAYBE_SelectActiveBrowser SelectActiveBrowser
#endif
IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       MAYBE_SelectActiveBrowser) {
  webapps::AppId app_id =
      InstallTestWebApp("/web_apps/basic.html", /*await_metric=*/false);
  EXPECT_EQ(GetLaunchHandler(app_id), std::nullopt);

  Browser* browser_1 = LaunchWebAppBrowser(app_id);
  Browser* browser_2 = LaunchWebAppBrowser(app_id);
  EXPECT_NE(browser_1, browser_2);

  {
    ScopedRegistryUpdate update = WebAppProvider::GetForTest(profile())
                                      ->sync_bridge_unsafe()
                                      .BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    web_app->SetLaunchHandler(LaunchHandler{ClientMode::kFocusExisting});
  }

  Browser* browser_3 = LaunchWebAppBrowser(app_id);
  // Select the most recently opened app window.
  EXPECT_EQ(browser_3, browser_2);
}

}  // namespace web_app
